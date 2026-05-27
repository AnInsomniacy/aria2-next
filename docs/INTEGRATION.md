# Integration Guide

This document describes the stable integration surface for applications, scripts, and JSON-RPC clients that use aria2-next.

## Stable Surfaces

The maintained public surfaces are the `aria2-next` executable, aria2-style command-line options, aria2-style configuration files, aria2-style session and input files, and JSON-RPC.

There is no maintained public C++ embedding API. Integrations should launch the executable or connect through JSON-RPC.

## Compatibility Model

Aria2 Next preserves aria2-compatible behavior where practical. Extensions are added only when compatibility fields cannot represent modern engine state safely.

Extensions should be consumed as optional fields. Unknown fields should be ignored by clients. Clients should not infer engine state from placeholder names, empty objects, or file paths when explicit fields are available.

## Proxy Contract

The `proxy-mode` option controls whether environment proxy settings are allowed.

`auto` is the CLI default. It allows configured proxy options and environment proxy variables.

`direct` means no proxy. Applications should pass this when the user explicitly selects no proxy.

`manual` uses only proxy options passed to aria2-next and ignores environment proxy variables.

## BitTorrent Metadata Contract

During magnet metadata download, `bittorrent.info` is omitted until stable torrent metadata exists. Clients should read `bittorrent.metadata.state` and `bittorrent.metadata.hasMetadata`.

The metadata state is `downloading` while libtorrent is downloading metadata or `hasMetadata` is false. The metadata state is `ready` when metadata exists.

If a magnet `dn` display name is available during metadata download, aria2-next exposes it as `bittorrent.metadata.displayName`. The authoritative torrent name remains `bittorrent.info.name` after real metadata is available.

Clients should not treat an empty `bittorrent.info` object as valid metadata.

## ED2K Progress Contract

ED2K low-level completed length can represent verified progress. `ed2k.visibleCompletedLength` exposes verified plus in-flight progress so paused, waiting, active, and stopped states do not visibly regress.

Clients should prefer the ED2K visible progress field for ED2K task display when present. HTTP, FTP, and BitTorrent progress semantics remain protocol-specific.

## HTTP Transfer Contract

Segmented HTTP transfers validate Range responses before writing body data. Invalid ranged responses are rejected before segment writes. Servers that ignore Range with `200 OK` can be downgraded to a single full-body transfer.

Clients should not treat the number of connections as fixed. aria2-next may adapt HTTP stream concurrency per origin to protect reliability.

## Checksum Contract

Whole-file checksums from input files or options are verified after the transfer completes. Completed files discovered through metadata probing still enter checksum validation when a checksum is pending.

Clients should wait for the final task result rather than assuming the transfer is complete when network bytes reach total length.

## Session and Control Files

Session and control files are owned by the engine. Applications can pass `--save-session`, `--save-session-interval`, and related options, but should not edit `.aria2` control files directly.

## Recommended Sidecar Options

Applications embedding aria2-next should pass explicit RPC binding options, an RPC secret, a clear proxy mode, a controlled session path, and debug logging paths for diagnostics.

For a no-proxy user setting, pass `--proxy-mode=direct`. For an app-managed proxy setting, pass `--proxy-mode=manual` and the explicit proxy options.

## Reporting Integration Bugs

Integration bugs should include the engine command line, JSON-RPC request and response, aria2-next version, platform, parent application version, and debug log. Redact secrets and private URLs.
