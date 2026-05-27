# Troubleshooting

This guide covers common aria2-next engine failures and the information needed to debug them.

## First Checks

Start with the binary version and enabled features.

```bash
aria2-next --version
aria2-next --help=#all
```

Run one failing task with debug logging.

```bash
aria2-next --no-conf --log-level=debug --console-log-level=debug --log=aria2-next.log <URI>
```

If a parent application starts the engine, reproduce with the standalone binary where possible.

## HTTPS Certificate Failures

Official Windows builds use the Windows certificate store through libcurl native CA support. Official macOS builds use Apple SecTrust. Official Linux builds use libcurl/OpenSSL CA auto-discovery and OpenSSL default fallback paths. Android shell environments may need an explicit CA path.

For custom CA bundles:

```bash
aria2-next --ca-certificate=/path/to/ca-bundle.pem https://example.com/file
```

Certificate errors caused by corporate TLS interception, proxy certificates, missing system roots, or expired server chains should be reported with the exact OpenSSL verify result from the log.

## Proxy Behavior

`--proxy-mode=auto` uses configured proxy options and environment proxy variables. This is the default CLI behavior.

`--proxy-mode=direct` disables proxy use for HTTP, HTTPS, and FTP transfers.

`--proxy-mode=manual` uses only explicitly configured aria2-next proxy options such as `--all-proxy`, `--http-proxy`, and `--https-proxy`.

For application integrations that expose an explicit "no proxy" mode, pass `--proxy-mode=direct`.

## HTTP Range and Large Downloads

Segmented HTTP downloads require valid byte-range responses. A valid ranged response uses HTTP `206 Partial Content`, a matching `Content-Range`, and identity encoding.

Some mirrors ignore Range and return `200 OK` with the full file. aria2-next detects this and downgrades the task to a single full-body download instead of writing full-body data into a segment.

If a large download fails, include log lines containing `Range`, `Content-Range`, `HTTP range request`, `Connection timed out`, or `CURLE_WRITE_ERROR`.

## Checksums and Input Files

Input files can include per-task checksums.

```text
https://example.com/file.iso
  out=file.iso
  checksum=sha-1=0123456789abcdef0123456789abcdef01234567
```

After the transfer completes, aria2-next schedules whole-file checksum validation. A successful checksum removes the control file unless options request otherwise. If validation is skipped, include the input-file entry, command line, final console output, and `.aria2` file state.

## Remote Timestamps

`-R` and `--remote-time=true` apply the server `Last-Modified` timestamp after a successful download.

If the timestamp is not applied, confirm the server returns `Last-Modified` and include the response headers or debug log.

## BitTorrent and Magnet Metadata

BitTorrent and magnet tasks are handled by libtorrent-rasterbar. During magnet metadata download, RPC status exposes metadata state under `bittorrent.metadata`. Real torrent metadata appears under `bittorrent.info` only after metadata is available.

For magnet issues, include the magnet link or a redacted equivalent, `--pause-metadata` setting, `tellStatus` output, and whether the process remains alive.

## ED2K

ED2K downloads need reachable servers or inline sources. Low source availability can look like a stalled download even when the engine is functioning.

For ED2K bugs, include the ED2K link, configured `--ed2k-server`, `--ed2k-server-list`, `--ed2k-node-list`, TCP/UDP listen ports, and debug log lines mentioning ED2K source discovery or peer transfer.

## Crashes

For Windows crashes, include the exception code, faulting module, fault offset, WER entry, and whether the build was a release or debug workflow artifact. If available, include a debugger stack trace.

For Linux or macOS crashes, include the signal, stderr, core dump notes, and the final debug log lines.

Do not upload dumps containing secrets, cookies, private URLs, or credentials without redaction.
