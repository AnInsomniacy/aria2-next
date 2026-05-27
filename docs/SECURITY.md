# Security Policy

Aria2 Next is a network-facing download engine. Reports involving crashes, remote inputs, TLS validation, proxy handling, RPC exposure, torrent parsing, ED2K parsing, or credential leakage should be handled carefully.

## Supported Versions

Only the latest published aria2-next release is actively supported for security fixes. Users should upgrade before reporting security behavior unless the report concerns a regression in the latest release.

## Reporting

Do not publish exploit details, private credentials, cookies, RPC secrets, proxy credentials, private trackers, signed URL tokens, or unredacted crash dumps in public issues.

Open a minimal public issue only when the report can be described without sensitive material. For sensitive reports, contact the maintainer through GitHub profile contact methods and provide a sanitized summary first.

## RPC Exposure

Do not expose JSON-RPC to untrusted networks without an RPC secret and network-level access control. Prefer loopback binding:

```bash
aria2-next --enable-rpc=true --rpc-listen-all=false --rpc-secret=<secret>
```

`--rpc-allow-origin-all=true` is intended for controlled environments. Browser-accessible RPC endpoints can expose local download control if misconfigured.

## TLS and Certificates

HTTPS certificate checking is enabled by default. Do not disable `--check-certificate` to work around unknown failures without understanding the network path.

Official Windows builds use the Windows certificate store. Official macOS builds use Apple SecTrust. Official Linux builds use libcurl/OpenSSL CA auto-discovery with OpenSSL fallback paths. Android shell users may need an explicit CA bundle.

## Logs and Dumps

Debug logs can contain URLs, headers, cookies, proxy settings, local paths, and private tracker details. Crash dumps can contain memory from command lines and network buffers. Redact sensitive data before sharing.

## Dependency Supply Chain

Release dependency sources and SHA-256 hashes are pinned in `packaging/dependencies.env`. Release workflows verify dependency archives before building.
