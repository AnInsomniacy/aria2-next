# RPC extensions

Aria2 Next serves the aria2 JSON-RPC and XML-RPC interface unchanged. Clients
written for aria2, such as AriaNg, keep working through the standard methods
and fields. This page lists what Aria2 Next adds on top, for frontends that
want to show ED2K, BitTorrent and media details or use the extra controls.

Conventions follow aria2: methods take the token as the first parameter when
`rpc-secret` is set, GIDs are 16 hex digits, and integer counters are decimal
strings. `system.listMethods` lists every method name.

## Discovering support

`aria2.getVersion` answers `{version, product, rpcVersion, enabledFeatures,
mediaFeatures, downloadFeatures}`. `product` is `aria2-next`; check it before
relying on anything below.

| Field | Values |
| --- | --- |
| `mediaFeatures` | `request-contexts`, `stable-track-ids`, `structured-errors`, `captured-inputs` |
| `downloadFeatures` | `filename-hints`, `filename-resolution` |

## Status fields

`tellStatus`, `tellActive`, `tellWaiting` and `tellStopped` return the aria2
fields and, by task kind, one more object. Standard fields keep their aria2
meaning; frontends that only read them see ordinary downloads.

| Object | Task kind | Main fields |
| --- | --- | --- |
| `media` | HLS and DASH | `state`, `protocol`, `live`, `duration`, `completedDuration`, `downloadedLength`, `progress`, `lengthKnown`, `tracks`, `error`, `errorCode` |
| `ed2k` | ED2K | `hash`, `name`, `length`, `completedLength`, `partHashCount`, `aichRoot`, `peerCount`, `queuedPeerCount`, `acceptedPeerCount`, `deadPeerCount`, `lowIdPeerCount`, `callbackWaitingPeerCount`, `sharingTime`, `corruptPartCount` |
| `bittorrent` (additions) | BitTorrent | `magnetLink`, `infoHashV1`, `infoHashV2`, `state`, `fileSelectionState`, `webSeeds`, `privateTorrent`, `numPeers`, `numSeeds`, `numComplete`, `numIncomplete`, `progress`, `availability`, `currentTracker`, `activeTime`, `finishedTime`, `seedingTime` |

A media download has no byte total until its output is written, so
`totalLength` stays `0` while it runs; use `media.progress` (completed
duration) and `media.downloadedLength` instead. Durations are milliseconds.
See [media downloads](media-downloads.md) for phases, tracks and errors.

`bittorrent.fileSelectionState` is `awaiting` while a magnet paused by
`pause-metadata` waits for `select-file`, `ready` once a selection is set but
the task is still paused, and `none` otherwise.

## Methods

### Downloads

| Method | Parameters | Result |
| --- | --- | --- |
| `aria2.probe` | `uri`, `options` | `{status, finalUrl, contentType, filename?, totalLength?, resumable, error?}` for the first byte, requested as a task with `options` would |
| `aria2.resolveFilename` | `url`, Content-Disposition as an array of bytes | The file name the engine would choose |
| `aria2.moveDownload` | `gid`, `dir` | `OK` once the files of an unfinished download are in `dir` |

`probe` and `moveDownload` are asynchronous: call them alone, not inside
`system.multicall`. See [architecture](architecture.md#rpc) for details.

### Media

| Method | Parameters | Result |
| --- | --- | --- |
| `aria2.finishMedia` | `gid` | Ends a live recording and finalizes what it has |
| `aria2.retryMedia` | `gid`, optional `options` | Requeues a failed media task with its recovery data |

To choose tracks, add with `media-pause-after-probe=true`, read
`media.tracks`, set the selection with `aria2.changeOption`, then unpause.

### BitTorrent

| Method | Parameters | Result |
| --- | --- | --- |
| `aria2.getBtTrackers` | `gid` | Trackers with their announce state, scrape counts and last error |
| `aria2.replaceBtTrackers` | `gid`, `[{url, tier}]` | `gid` |
| `aria2.replaceBtWebSeeds` | `gid`, `[url]` | `gid` |
| `aria2.addBtPeers` | `gid`, `["host:port"]` | `{added, failed}` |
| `aria2.forceBtAnnounce` | `gid` | `gid`; announces to trackers, DHT and local discovery |
| `aria2.forceBtRecheck` | `gid` | `gid`; hashes the downloaded data again |
| `aria2.setBtPeerBlocklist` | `[rule]` (address, CIDR block or range) | `{ruleCount, revision}`; replaces the list, an empty array clears it, one invalid rule rejects the call |
| `aria2.getBtSessionStatus` | | Session-wide BitTorrent state |
| `aria2.inspectTorrent` | base64 `.torrent` | `{name, mode, totalLength, infoHashV1, infoHashV2, files}` without adding a task |

### ED2K

| Method | Parameters | Result |
| --- | --- | --- |
| `aria2.ed2kSearch` | `keyword`, optional `{fileType, extension, minSize, maxSize, minSourceCount, minCompleteSourceCount, timeout}` | Search GID; `timeout` is 5 to 600 seconds, 60 by default |
| `aria2.getEd2kSearchResults` | search GID | `{gid, status, moreResults, results: [{hash, name, length, sourceCount, completeSourceCount, ed2kLink, ...}]}` |
| `aria2.ed2kSearchMore` | search GID | `true` when the server was asked for more |
| `aria2.removeEd2kSearch` | search GID | `OK` |
| `aria2.getEd2kStatus` | | `clientId`, `userHash`, `tcpPort`, `udpPort`, `portMapping`, upload and credit counts |

## Links

`thunder://` links are accepted wherever a URI is, by `aria2.addUri`,
`aria2.probe`, the command line and sessions. The task holds the URI the link
carried.
