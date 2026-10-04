# Internal architecture

Aria2 Next keeps the CLI, JSON/XML-RPC methods, libaria2 API and persisted task
formats at their existing boundaries. Internal source files follow the owner of
the behavior they implement. A directory does not imply another runtime layer
or independently linked library.

## Ownership map

| Area | Responsibility | Primary owner |
| --- | --- | --- |
| `options/` | Register supported options and their validation/defaults | `OptionCatalog` and `OptionParser` |
| `rpc/` | Validate RPC input, invoke task operations and project response fields | Method classes and `RpcMethodFactory` |
| `task/` | Queue activation, stopping, result retention, storage and notifications | `RequestGroup` and `RequestGroupMan` |
| `transport/` | Native libcurl resources and shared HTTP response parsing | `CurlMulti` and focused functions |
| `stream/` | Request setup, byte-range scheduling, completion and output persistence | `CurlSession`, `CurlDownload` and `CurlHandle` |
| `bittorrent/` | Native torrent configuration, alerts, selection and resume data | `BtSession` and `BtDownload` |
| `ed2k/` | Peer/server packet handling, Kad operations and ED2K runtime state | Existing command and attribute types |
| `media/` | Manifest playback, retained segments and final remux publication | `MediaJob`, `Transport`, `Store` and mux resource owners |
| `platform/` | Native socket, process, text and OS resource behavior | `SocketCore` and focused functions |
| `support/` | Text, numbers, encoding, paths and other bounded utilities | Stateless functions where possible |

Stable command, event, disk and protocol interfaces remain in `src/`. Public
library headers remain under `src/includes/aria2/`. Source registration and
feature guards live in `cmake/Sources.cmake` and `cmake/TestSources.cmake`.

## Lifecycle constraints

- The engine thread owns task coordination, libcurl event subscriptions and
  BitTorrent alert dispatch. Native alert pointers are borrowed only during
  dispatch; asynchronous results enter task state through the existing alerts.
- `CurlMulti` owns multi/share resources and socket/timer registrations.
  `CurlHandle` owns its easy handle and header list. Remove an easy handle from
  the multi handle before destroying it. Copy borrowed native result strings
  before releasing the easy handle.
- Stream output creation, writes, allocation, close and recovery checkpoints run
  on a bounded Boost.Asio pool, serialized per task. The engine polls completions
  without waiting; only successful writes commit planner ranges. libcurl pause/resume
  supplies backpressure. Pausing, restarting and final results wait for pending I/O;
  an output error wins over a pending pause. SQLite checkpoints are transactional,
  and a committed in-memory index keeps task lookup off the database write lock.
- Stream leases use half-open byte ranges `[begin, end)`. Media HTTP range
  endpoints are inclusive. Keep conversions explicit at the protocol boundary.
- Persist completed output and recovery state before releasing task runtime
  resources or publishing final results. A pause returns a task to the waiting
  queue with its recovery identity intact. Queue mutations commit through
  `RequestGroupMan::saveSession`; failed writes retain the previous hash and
  retry without replaying stop hooks. Stream recovery survives completion until
  the session no longer references the task.
- BitTorrent add alerts carry `add_torrent_params::userdata` back to their owning
  `BtDownload`. An info hash identifies content, not an asynchronous request.
- `stream-max-range-size` caps requests at the libcurl handle boundary. Validator
  changes or encoded range responses can restart an owned partial file with a fresh
  full request. Encoded ranges are rejected before writes; full responses use native
  libcurl decoding and track decoded output length. Interrupted full responses have
  a bounded restart budget (`max-tries`, or three restarts when unlimited was requested);
  they never join old bytes to a new representation or override a range cap.
- Each media worker owns its GPAC client, transport cache and publication
  transaction. `DashFileIo` borrows that worker. Only the shared `Control` crosses
  threads; its snapshot is protected by its mutex.
- Media segment positions and durations use milliseconds. Native packet clocks
  retain their stream time bases; mux offsets use microseconds. Convert units
  explicitly rather than sharing unlabelled integer timestamps.

## Native integration boundaries

libcurl handles connection reuse, transport and asynchronous socket activity.
The stream scheduler owns file-range assignments and persistence. libtorrent
owns BitTorrent networking, piece management and native resume data. GPAC owns
HLS/DASH manifest timing and segment availability; FFmpeg owns demuxing and
remuxing. SQLite owns transactional state storage.

Do not replace these boundaries with another event bus, generic transport
framework or catch-all utility header. Share functions only where both the
behavior and ownership requirements match. In particular, media and ordinary
range downloads intentionally have different request and retry policies.

## Output naming

`support/OutputName` owns untrusted name decoding and filename suggestions.
`stream/StreamStorage` binds the chosen name to the real output and native recovery
state. New implicit HTTP names wait for the final payload headers; redirects and
rejected responses never create provisional files. libcurl's native header and
URL information provide the accepted response metadata. New tasks reserve an unused
path, including active-task reservations, and create it exclusively. Recovery requires
the same persisted GID, URL and path; matching file sizes never establish ownership.
`continue` does not adopt an unrelated file. A full restart can truncate only an
output created or recovered by that task (or an explicitly permitted overwrite).

Precedence is explicit `out` or persisted path, browser-resolved hint, final
Content-Disposition, suggested hint, original URL `filename=` query, final URL
`filename=` query, final URL basename, then the default.
Content-Disposition uses the existing aria2 parser. Browser names and persisted
paths are already text and are never URL decoded. Final names are persisted in
native task options so restart does not choose another path.

Media titles append the selected container extension; filename hints replace their
extension. The media worker retains ownership of collision handling and publication.
`aria2.resolveFilename(url, contentDispositionBytes)` returns a suggested basename
through the same resolver without creating a task or downloading a body. The second
parameter contains the original header bytes (integers 0–255, up to 8192 bytes).
`aria2.getVersion` advertises `filename-hints` and `filename-resolution` in `downloadFeatures` so consumers
can require this contract before handing off a task. The public aria2 adapter and
its option/link mappings remain intact.

## Verification boundaries

CTest runs the native doctest suite. Keep regressions for parsing, state
transitions, ownership, data integrity and public contracts. Fixtures use
ordinary construction/destruction; `a2doctest.h` only supplies diagnostic
formatting. See the [test ownership map](../tests/README.md) for scenario and
fixture boundaries.

`tools/transfer_validation/run.py all` exercises the executable over CLI/RPC
against local HTTP, SFTP, BitTorrent, ED2K, Metalink and media fixtures. Public
downloads are separate interoperability checks: a remote service failure is
recorded with its evidence and does not become a passing result or an automatic
diagnosis of an engine defect. See [transfer validation](../tools/transfer_validation/README.md)
and [public media validation](media-e2e.md).
