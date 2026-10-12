# Architecture

The core under `src/` is one static library, `aria2_core`, linked into the
`aria2-next` executable and the doctest suite under `tests/`. It keeps the
aria2 command line, JSON-RPC methods and session file format at their
boundaries; everything behind them is organised as below.

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
| `net/` | UPnP IGD port mapping (`PortMapper`) |
| `media/` | HLS/DASH tasks |
| `app/` | command-line entry point, console readout, daemon |

## HTTP transfer

A task keeps one interval set of received bytes and a list of workers. A
worker is one libcurl transfer that owns `[position, end)` of the file.

1. The first request asks for `bytes=0-`. Its response establishes length,
   range support and the validator (strong ETag, else Last-Modified). A
   first request without an answer after 2 s gets a second copy (a server
   that accepts and stays silent); the first answer wins.
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
   `max-tries` consecutive failures without progress. With several mirrors
   (Metalink) a failing or overloaded mirror backs off alone and the
   connection moves to another; `max-tries` applies per mirror.
6. Every response is checked against the established length and validator,
   and Content-Range must match the request. A mismatching single response is
   retried; a changed representation restarts a task-owned file from zero and
   fails a foreign file without modifying it. A server that ignores ranges
   gets one connection from offset zero. Mirrors keep their own validators
   (each mirror's ETag differs); a mirror that ignores ranges or changes the
   file is dropped while another mirror remains.
7. Parallel ranges use HTTP/1.1 so each range has its own TCP window; single
   transfers may negotiate HTTP/2.
8. A Metalink's `<size>` is binding: a mirror reporting another length is
   dropped. When the whole-file hash fails and the Metalink has `<pieces>`,
   only the failing pieces are downloaded again (up to three rounds), and a
   mirror that alone delivered a failing piece is dropped.

Bytes are buffered per worker and written by the disk pool, in chunks or at
least once a second, so slow connections do not hold unsaved data. The interval set
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

## Media

An HLS or DASH presentation is one task with one output file. GPAC's DASH
client reads manifests and schedules segments; FFmpeg remuxes them. Both run
on a worker thread per task with a frozen copy of the task options. The
worker owns its libcurl multi handle (segment prefetch, request contexts,
captured manifests) and shares nothing with the control thread except a
`Control` block: atomics for cancel, finish and counters, plus a snapshot
under a mutex that the task polls.

1. A URI is media when `media` names a protocol, or when `media=auto` and
   its path ends in `.m3u8`/`.mpd`. An HTTP task whose first response has a
   manifest Content-Type hands its slot to a media task with the same GID.
2. Segments are committed to `media/state.db` only after their bytes are
   durable, so a pause, crash or restart resumes from the last segment.
   Publication renames a staged file into place and is recorded first, so it
   is also restartable.
3. With `media-pause-after-probe` the task pauses with
   `media.state=awaiting-selection` once tracks are known; a client chooses
   tracks with `changeOption` and unpauses it.
4. `aria2.finishMedia` ends a live recording after its current segment;
   `aria2.retryMedia` requeues a failed task, keeping its segments.

## ED2K

One `Network` per engine, started by the first ED2K task: TCP and UDP
listeners, the server session, Kad, the upload queue and keyword searches.
aMule is the behavioural reference; checked against it on the wire in both
directions (plain, compressed, obfuscated).

1. Files are split into 9.28 MB parts hashed with MD4; sources claim 180 KB
   blocks, rarest part first, and duplicate a block only in the endgame. A
   complete local file without a part hash set hashes its parts and adopts
   them when their root matches the file hash.
2. Uploads deflate each block on the disk thread and send it compressed when
   it shrinks; incompressible files are probed only every 32 blocks.
3. A few servers are dialled at once, one more per second, and the first to
   connect logs in. Searches wait until the server or Kad can take them.
4. Kad lookups walk until the closest 8 contacts that did not fail have
   answered, search contacts within tolerance as soon as they answer, and
   widen when no answer came for 2 s. The routing table splits the far half
   of the ID space into 8 buckets, the next quarter into 4 and the eighth
   after it into 2, as eMule's zones do down to level 4.
5. A known client that calls in is identified by its hello; the existing
   object takes the connection over and the reading coroutine forwards to it.
6. With `ed2k-port-mapping` the TCP and UDP ports are forwarded by UPnP
   (`net/PortMapper`); mappings are removed before the engine stops.

## RPC

aria2 JSON-RPC over HTTP POST and WebSocket, with `system.multicall`,
notifications and the token secret. XML-RPC and JSONP are not provided.
Extension methods keep their existing names and fields.

`aria2.probe(uri, options)` requests the first byte of an HTTP(S) URL with
the request settings a task with `options` would use and answers
`{status, finalUrl, contentType, filename?, totalLength?, resumable, error?}`
once the response headers arrive (at most 20 s). Clients use it to decide
whether the engine can take over a download a browser started. It is the
only asynchronous method: it must be called alone, not in a batch or
`system.multicall`, and other requests on the same connection are answered
meanwhile only over WebSocket.

## State

- Session file: aria2 input-file format, written atomically.
- `stream/state.db` (SQLite): HTTP/SFTP resume records (ranges, validators,
  identity of the output file).
- `bittorrent/`: see above. `ed2k/state.db`: ED2K identity and part metadata.
