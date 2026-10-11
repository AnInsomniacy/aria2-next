# Engine architecture

This directory holds the replacement core. It becomes `src/` once it reaches
parity with the transfer validation suite; the legacy core is then deleted.

## Principles

- Mature libraries own their domains: Boost.Asio (event loop), Boost.Beast and
  Boost.JSON (RPC), libcurl (HTTP/HTTPS/SFTP), libtorrent (BitTorrent),
  GPAC/FFmpeg (HLS/DASH), SQLite (state), OpenSSL (hashes, TLS), spdlog (logs).
- Hand-written code is limited to what no library provides: segment
  scheduling, task/queue semantics, ED2K, and the aria2-compatible surface.
- One fact, one owner. A task owns its files and progress; the engine owns
  the queue; RPC only translates.
- Failures are explicit and local. No watchdogs that restart healthy work.

## Threads

| Thread | Work |
| --- | --- |
| control (`Engine::io`) | tasks, queue, timers, RPC, libcurl multi, ED2K sockets |
| disk pool | positional writes and reads, fsync, hashing |
| libtorrent | its own session threads; alerts are posted to control |
| media workers | GPAC/FFmpeg pipelines; progress is posted to control |

Everything that touches task state runs on the control thread, so task code
has no locks. Other threads communicate by posting completions to it.

## Modules

| Module | Responsibility |
| --- | --- |
| `base/` | logging, errors, text, numbers, paths, URI helpers |
| `options/` | option catalogue, CLI and config parsing, typed access |
| `core/` | `Engine`, `Task`, queue, GID, events, rate limiting, statistics |
| `disk/` | `DiskPool`, `OutputFile`, output naming and conflicts |
| `http/` | libcurl on Asio, `HttpTask`, `SegmentPlanner`, probe, Metalink |
| `state/` | session file, SQLite resume store |
| `rpc/` | Beast HTTP/WebSocket server, JSON-RPC dispatch, methods |
| `bt/` | libtorrent session and `BtTask` |
| `ed2k/` | protocol codec and client, server, Kad, queues |
| `media/` | HLS/DASH tasks |
| `app/` | command-line entry point, console readout, daemon |

## HTTP transfer

A task keeps one interval set of received bytes and a list of workers. A
worker is one libcurl transfer that owns `[position, end)` of the file.

1. The first request asks for `bytes=0-`. Its response establishes length,
   range support and the validator (strong ETag, else Last-Modified).
2. Idle connection slots take the largest unowned interval, or split the
   worker with the longest expected remaining time. The split point balances
   both halves by their measured speeds. Small remainders are not split.
3. A worker that reaches its end while the next worker has not received a
   byte takes over that interval instead of opening another connection.
4. A worker that receives nothing for the stall window is aborted and its
   remainder returns to the unowned set. In the endgame a slow worker is
   raced by a duplicate request; the first to finish wins, the other is
   cancelled. Identical bytes may be written twice; that is harmless.
5. Errors affect one worker. 429/503 lower the connection cap to the number
   of workers currently transferring and honour Retry-After; the cap probes
   upward again after a stable period. A task fails only after
   `max-tries` consecutive failures without progress.
6. Every response is checked against the established length and validator,
   and Content-Range must match the request. A mismatching single response is
   retried; a changed representation restarts a task-owned file from zero and
   fails a foreign file without modifying it. A server that ignores ranges
   gets one connection from offset zero.
7. Parallel ranges use HTTP/1.1 so each range has its own TCP window; single
   transfers may negotiate HTTP/2.

Bytes are buffered per worker and written by the disk pool. The interval set
distinguishes received from persisted bytes; checkpoints store only persisted
ranges. When unpersisted bytes exceed the write budget, transfers pause until
the pool catches up.

## BitTorrent

One libtorrent session serves all tasks. Its alerts are drained on the
control thread; `post_torrent_updates()` refreshes every running torrent's
cached status once a second, so status queries never wait for libtorrent.

1. The engine queue decides what runs. A stopping task pauses its torrent,
   saves resume data (with the info dictionary), waits until the file is on
   disk, then removes the torrent from the session. Starting adds it again
   from that resume data. No torrent is auto-managed.
2. Magnet links are added with every file deselected, so no payload arrives
   before the selection is known. With `pause-metadata` over RPC the task
   pauses once metadata arrives and reports `fileSelectionState=awaiting`
   until a client sets `select-file` and unpauses it.
3. A finished download stays active while seeding and completes when
   `seed-time` or `seed-ratio` is reached, whichever comes first;
   `seed-time=0` completes it immediately. Both are live options, so a client
   ends seeding with `changeOption`.
4. `check-integrity` adds no work: libtorrent verifies resume data and hashes
   existing files when there is none. `hash-check-only` drops the resume data
   so every existing piece is hashed, then completes.
5. Global speed limits apply to libtorrent and to HTTP separately.

State below `--state-dir/bittorrent` keeps the layout of earlier releases:
`session` (DHT nodes), `torrents/<sha1>.torrent` (metainfo of tasks added
from torrent data, named in the session file) and
`torrents/<gid>.fastresume`. Resume files that earlier releases named after
the info hash are used only for the same task in the same directory.

## RPC

aria2 JSON-RPC over HTTP POST and WebSocket, with `system.multicall`,
notifications and the token secret. XML-RPC and JSONP are not provided.
Extension methods keep their existing names and fields.

## State

- Session file: aria2 input-file format, written atomically.
- `stream/state.db` (SQLite): HTTP/SFTP resume records (ranges, validators,
  identity of the output file).
- `bittorrent/`: see above. `ed2k/state.db`: ED2K identity and part metadata.
