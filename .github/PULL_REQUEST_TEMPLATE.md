<!--
Before submitting, read docs/CONTRIBUTING.md.

PR titles must use Conventional Commits.
Examples:
  fix(http): retry transient segmented transfer failures
  feat(rpc): expose ED2K visible progress
  docs: add release troubleshooting guide
  ci(release): add Windows debug artifacts
-->

## Description

<!-- What does this change do and why? Link issues with Fixes #123 where applicable. -->

## Affected surface

<!-- Check all that apply. -->

- [ ] CLI options or configuration
- [ ] JSON-RPC behavior
- [ ] HTTP / HTTPS / FTP / SFTP transfers
- [ ] BitTorrent / magnet / libtorrent
- [ ] ED2K
- [ ] Session, input file, or control file handling
- [ ] Checksum or integrity verification
- [ ] Build, dependency, or release packaging
- [ ] Documentation only

## Compatibility

<!-- Describe any aria2-compatible behavior change, aria2-next extension, or Motrix Next integration impact. -->

## Verification

<!-- Paste the exact commands run. "It builds" is not sufficient. -->

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

## AI usage disclosure

- [ ] No AI tools were used.
- [ ] AI tools assisted with drafting, refactoring, or boilerplate. I reviewed and understand every line.
- [ ] Substantial portions were AI-generated. I reviewed, tested, and can explain every change.

Tool used, if any:

## Checklist

- [ ] I read `docs/CONTRIBUTING.md`.
- [ ] The PR addresses one concern only.
- [ ] User-facing behavior changes are documented.
- [ ] New or changed CLI/RPC behavior has focused tests.
- [ ] Packaging changes were verified with the affected release path or script syntax checks.
- [ ] Dependency version or hash changes update `packaging/dependencies.env` and affected documentation.
- [ ] No generated build output, binaries, logs, dumps, or local caches are committed.
- [ ] Commits follow Conventional Commits.

## Release notes

<!-- One user-facing sentence, or "none" if not user-facing. -->
