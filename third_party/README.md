# Vendored Dependencies

This directory contains the curated source required to build aria2-next
without downloading library dependencies during configuration or compilation.

| Dependency | Version | Integration |
| --- | --- | --- |
| Boost | 1.91.0 | Header-only asynchronous networking used by the core and libtorrent |
| curl | 8.21.0 | Static HTTP, HTTPS, and SFTP transfer engine |
| doctest | 2.4.12 | Header-only unit test framework |
| Expat | 2.8.1 | Static XML parser |
| FFmpeg | 8.1.2 | Static media demuxing, bitstream filters, and MP4/Matroska muxing |
| GPAC | 26.07.0 | Static native HLS/DASH client |
| libssh2 | 1.11.1 | Static SFTP transport |
| libtorrent-rasterbar | 2.1.1 | Static BitTorrent engine |
| nghttp2 | 1.70.0 | Static HTTP/2 framing library used by curl |
| OpenSSL | 3.5.6 | Static TLS and cryptography library |
| spdlog | 1.17.0 | Header-only logging library |
| SQLite | 3.53.1 | Static persistent state database |
| wslay | 1.1.1 | Static WebSocket implementation |
| zlib | 1.3.2 | Static compression library |

The default CMake superbuild compiles these sources into an isolated prefix
under the selected build directory. It never searches Homebrew, system package
directories, or a network dependency provider for these libraries.

Vendored directories contain no nested Git repositories or submodules.
Dependency versions are updated manually together with
`packaging/dependencies.env`.

Vendored trees retain the upstream build files, source, headers, and license
material used by the maintained CMake configurations. Alternate build systems,
examples, tests, tools, unsupported crypto backends, and unsupported platform
ports are removed.

curl and nghttp2 contain only their library sources, public headers, CMake
integration, release metadata, and license material. Their command-line tools,
servers, examples, tests, fuzzers, and nested repository metadata are excluded.

Boost is a header subset for the maintained Boost.Asio and libtorrent
configuration. Disabled WebTorrent, I2P, and fallback cryptography include
trees are excluded.

The libtorrent tree carries an aria2-next setting extension for encrypted-first
peer negotiation with libtorrent's native plaintext fallback.
It also carries an extension for per-task HTTP headers on BitTorrent web seeds.

GPAC is configured without a player, renderer, JavaScript runtime, external
media codecs, or independent TLS and cryptography backends. Its public DASH client uses aria2-next's
libcurl I/O adapter. The retained upstream source and build support cover this
configuration. A local client fix preserves the first queued segment's timing
and discontinuity and uses parsed HLS media sequence numbers instead of numbers
inferred from filenames. Terminal manifest I/O errors return to the caller,
including cancellation, instead of retrying the same xlink indefinitely.
Its threading header includes the standard integer
types required by its native atomic helpers.

Client integration fixes apply startup quality before initialization, preserve
HLS rendition languages and initialization key scope, and retain standalone AAC
and WebM download support. Explicit MP4 index ranges avoid incremental header
requests. Fixed HLS downloads refresh only selected renditions. HLS resume
addresses parsed media sequences; DASH clock synchronization
uses the clock response's receive time. The superbuild checks GPAC's native
incremental build on every build and preserves unchanged installed headers.

DASH recovery uses native Period and timeline positions. Client fixes preserve
individual segment durations, infer closed Period durations on refresh, and wait
for empty future timelines without timestamp underflow. Native seeking respects
trimmed timelines and their end. Timeline selection retains the actual segment
count and handles a first audio sample just after the Period start. The adapter reports delivery outcomes to GPAC;
bounded live retries cannot silently skip media, and ordinary HLS reloads respect
the parsed target duration.
SegmentTimeline recovery retains the MPD/UTC availability epoch. The upstream
startup heuristic that moved this epoch based on the selected segment is removed:
with a negative clock correction and an epoch-based MPD, it underflowed the
unsigned timestamp and scheduled the next request weeks into the future.
The native WebVTT parser and its timed-text/import dependencies are enabled for
ISO WebVTT samples. GPAC supplies cue payloads, identifiers and settings to the
FFmpeg packet adapter; no separate subtitle parser or media executable is used.

On Windows, static-only zlib builds retain the `libz` name used by upstream
pkg-config metadata and the maintained dependency consumers. A suffix is only
needed when static and shared zlib are built together.

FFmpeg is built without programs, network protocols, encoders, devices, or
filters. Only media demuxers, MP4/Matroska muxers, codec parsers, the audio
decoders and libswresample needed for reliable stream probing, and required bitstream filters
are enabled. Its upstream makefile support remains intact, while unused FATE
reference data and integration fixtures are omitted. Dependency source commits
are recorded in `packaging/dependencies.env`.
