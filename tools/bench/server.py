"""Adversarial HTTP origin for download benchmarks.

Serves a synthetic file whose 8-byte words encode their own index, so any
misplaced or duplicated byte range is detectable without storing the payload.
Faults are selected per request through query parameters:

  size=N          file size in bytes (multiple of 8)
  rate=B          per-connection throughput in bytes/s
  iprate=B        throughput shared by all connections to one listen address
  maxconn=K       concurrent responses per client beyond K get 503
  code=429        status used by maxconn (429 or 503)
  retryafter=S    Retry-After seconds sent with maxconn rejections
  stallat=O       the first response that reaches offset O stops sending
  resetat=O       the first response that reaches offset O is reset
  slowtail=F      responses starting in the last fraction F run at rate/20
  badrange=1      the first ranged response reports a shifted Content-Range
  norange=1       ignore Range and always send 200 with the full body
  expire=S        /signed/... redirects to a token valid for S seconds

One-shot faults trip once per path; benchmarks use a fresh path per run.

Run: python server.py --hosts 127.0.0.1,127.0.0.2 --port 18080
"""

from __future__ import annotations

import argparse
import asyncio
import functools
import secrets
import time
from array import array
from urllib.parse import parse_qs, urlsplit

CHUNK = 1 << 16


@functools.lru_cache(maxsize=256)
def _block(index: int) -> bytes:
    start = index * (CHUNK // 8)
    return array("Q", range(start, start + CHUNK // 8)).tobytes()


def payload(offset: int, length: int) -> bytes:
    """Returns file bytes [offset, offset + length)."""
    out = bytearray()
    while length > 0:
        index, skip = divmod(offset, CHUNK)
        piece = _block(index)[skip : skip + length]
        out += piece
        offset += len(piece)
        length -= len(piece)
    return bytes(out)


def verify(path: str, size: int) -> int | None:
    """Returns the first wrong offset of a downloaded file, or None."""
    with open(path, "rb") as file:
        offset = 0
        while offset < size:
            data = file.read(1 << 20)
            if not data:
                return offset
            if data != payload(offset, len(data)):
                for at in range(0, len(data), 8):
                    if data[at : at + 8] != payload(offset + at, 8):
                        return offset + at
            offset += len(data)
        return None if not file.read(1) else size


class Bucket:
    """Token bucket shared by the connections it throttles."""

    def __init__(self, rate: float):
        self.rate = rate
        self.tokens = 0.0
        self.stamp = time.monotonic()

    async def take(self, amount: int) -> None:
        while True:
            now = time.monotonic()
            self.tokens = min(self.rate / 4, self.tokens + (now - self.stamp) * self.rate)
            self.stamp = now
            if self.tokens >= amount:
                self.tokens -= amount
                return
            await asyncio.sleep((amount - self.tokens) / self.rate)


class Origin:
    def __init__(self) -> None:
        self.ip_buckets: dict[tuple[str, float], Bucket] = {}
        self.active: dict[str, int] = {}
        self.tripped: set[tuple[str, str]] = set()
        self.tokens: dict[str, float] = {}
        self.stats = {"requests": 0, "rejected": 0, "bytes": 0}

    async def handle(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        local = writer.get_extra_info("sockname")[0]
        client = writer.get_extra_info("peername")[0]
        try:
            while True:
                head = await reader.readuntil(b"\r\n\r\n")
                lines = head.decode("latin-1").split("\r\n")
                method, target, _ = lines[0].split(" ", 2)
                headers = {}
                for line in lines[1:]:
                    if ":" in line:
                        name, value = line.split(":", 1)
                        headers[name.strip().lower()] = value.strip()
                keep = await self.respond(method, target, headers, local, client, writer)
                if not keep:
                    break
        except (asyncio.IncompleteReadError, ConnectionError, asyncio.LimitOverrunError):
            pass
        finally:
            writer.close()

    async def respond(self, method, target, headers, local, client, writer) -> bool:
        self.stats["requests"] += 1
        url = urlsplit(target)
        query = {key: values[-1] for key, values in parse_qs(url.query).items()}
        path = url.path

        if path.startswith("/signed/"):
            ttl = float(query.get("expire", "5"))
            token = secrets.token_hex(8)
            self.tokens[token] = time.monotonic() + ttl
            location = f"/s/{token}/{path[len('/signed/'):]}?{url.query}"
            return await self.simple(writer, 302, {"Location": location})
        if path.startswith("/s/"):
            token = path.split("/")[2]
            if self.tokens.get(token, 0) < time.monotonic():
                return await self.simple(writer, 403)

        size = int(query.get("size", str(256 << 20)))
        start, end, partial = 0, size - 1, False
        spec = headers.get("range", "")
        if spec.startswith("bytes=") and query.get("norange") != "1":
            first, _, last = spec[6:].partition("-")
            start = int(first)
            end = min(int(last), size - 1) if last else size - 1
            if start >= size or start > end:
                return await self.simple(writer, 416, {"Content-Range": f"bytes */{size}"})
            partial = True

        limit = int(query.get("maxconn", "0"))
        key = f"{client}|{url.path}"
        if limit and self.active.get(key, 0) >= limit:
            self.stats["rejected"] += 1
            extra = {}
            if "retryafter" in query:
                extra["Retry-After"] = query["retryafter"]
            return await self.simple(writer, int(query.get("code", "503")), extra)

        reported = start
        if partial and start > 0 and query.get("badrange") == "1" and (path, "badrange") not in self.tripped:
            self.tripped.add((path, "badrange"))
            reported = start + 4096
        length = end - start + 1
        status = "206 Partial Content" if partial else "200 OK"
        lines = [
            f"HTTP/1.1 {status}",
            f"Content-Length: {length}",
            "Content-Type: application/octet-stream",
            'ETag: "bench-' + str(size) + '"',
            "Last-Modified: Mon, 01 Jan 2024 00:00:00 GMT",
            "Accept-Ranges: bytes" if query.get("norange") != "1" else "Accept-Ranges: none",
            'Content-Disposition: attachment; filename="bench.bin"',
        ]
        if partial:
            lines.append(f"Content-Range: bytes {reported}-{reported + length - 1}/{size}")
        writer.write(("\r\n".join(lines) + "\r\n\r\n").encode())
        if method == "HEAD":
            await writer.drain()
            return True

        rate = float(query.get("rate", "0"))
        if "slowtail" in query and start >= size * (1 - float(query["slowtail"])) and (path, "slowtail") not in self.tripped:
            self.tripped.add((path, "slowtail"))
            rate = (rate or 8 << 20) / 20
        own = Bucket(rate) if rate else None
        shared = None
        if "iprate" in query:
            shared = self.ip_buckets.setdefault((local, float(query["iprate"])), Bucket(float(query["iprate"])))
        stall = int(query.get("stallat", "-1"))
        reset = int(query.get("resetat", "-1"))

        self.active[key] = self.active.get(key, 0) + 1
        try:
            offset = start
            while offset <= end:
                count = min(CHUNK, end + 1 - offset)
                if start <= stall < offset + count and (path, "stall") not in self.tripped:
                    self.tripped.add((path, "stall"))
                    await asyncio.sleep(3600)
                if start <= reset < offset + count and (path, "reset") not in self.tripped:
                    self.tripped.add((path, "reset"))
                    writer.transport.abort()
                    return False
                if own:
                    await own.take(count)
                if shared:
                    await shared.take(count)
                writer.write(payload(offset, count))
                await writer.drain()
                offset += count
                self.stats["bytes"] += count
        finally:
            self.active[key] -= 1
        return True

    async def simple(self, writer, code: int, extra: dict[str, str] | None = None) -> bool:
        reasons = {302: "Found", 403: "Forbidden", 416: "Range Not Satisfiable", 429: "Too Many Requests", 503: "Service Unavailable"}
        lines = [f"HTTP/1.1 {code} {reasons.get(code, 'Error')}", "Content-Length: 0"]
        lines += [f"{name}: {value}" for name, value in (extra or {}).items()]
        writer.write(("\r\n".join(lines) + "\r\n\r\n").encode())
        await writer.drain()
        return True


async def serve(hosts: list[str], port: int) -> None:
    origin = Origin()
    servers = [await asyncio.start_server(origin.handle, host, port, limit=1 << 16) for host in hosts]
    print(f"serving on {', '.join(hosts)} port {port}", flush=True)
    await asyncio.gather(*(server.serve_forever() for server in servers))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--hosts", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=18080)
    args = parser.parse_args()
    asyncio.run(serve(args.hosts.split(","), args.port))


if __name__ == "__main__":
    main()
