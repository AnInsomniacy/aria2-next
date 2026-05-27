# Privacy Policy

Last updated: 2026-05-27

Aria2 Next is an open-source command-line download engine. It does not collect telemetry, analytics, usage profiles, account data, advertising identifiers, or crash reports.

## Local Data

Aria2 Next can create local files based on user options:

| Data | Purpose |
| --- | --- |
| Downloaded files | User-requested output |
| `.aria2` control files | Resume and progress state |
| Session files | Saved active, waiting, paused, or unfinished tasks |
| Log files | Diagnostics when logging is enabled |
| Cookie files | HTTP cookie input or output when configured |
| Server stat files | URI selector statistics when configured |
| ED2K state in sessions | ED2K server, Kad, shared file, and credit state when saved |
| BitTorrent resume data | libtorrent-owned torrent resume metadata |

These files remain on the user's device unless the user or another application shares them.

## Network Connections

Aria2 Next connects to network endpoints required by user-requested tasks. This can include HTTP, HTTPS, FTP, SFTP, BitTorrent trackers, BitTorrent peers, DHT nodes, ED2K servers, ED2K peers, and JSON-RPC clients.

Aria2 Next does not contact project servers for telemetry or analytics.

Official release workflows download dependency source archives from the URLs pinned in `packaging/dependencies.env`. That behavior belongs to project CI and packaging, not the runtime binary.

## Proxies

When `--proxy-mode=auto` is used, aria2-next can use configured proxy options and environment proxy variables. When `--proxy-mode=direct` is used, proxy use is disabled. When `--proxy-mode=manual` is used, only explicit aria2-next proxy options are used.

Proxy servers can observe traffic according to their protocol and TLS interception behavior.

## Logs

Logs may contain URLs, file paths, response headers, proxy settings, peer endpoints, tracker URLs, and error messages. They can contain secrets if secrets are passed in URLs or headers.

Before sharing logs publicly, redact RPC secrets, cookies, authorization headers, proxy credentials, private tracker URLs, signed URL tokens, and private local paths.

## Parent Applications

Applications such as Motrix Next may start aria2-next as a sidecar and manage additional data such as preferences, history, update metadata, and diagnostics. See the parent application's privacy policy for that data.

## Changes

Updates to this policy are made in this repository.
