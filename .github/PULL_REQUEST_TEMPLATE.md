<!--
Before submitting, read docs/CONTRIBUTING.md.

Keep this template complete. PRs with deleted sections, vague answers, or unchecked required items may be closed without review.

PR title must follow Conventional Commits format.

Good titles:
  fix(http): handle stalled async DNS resolution on Windows
  feat(ed2k): add direct callback request handling
  docs: clarify Docker runtime volumes
  ci(release): tighten Android dependency checks

Bad titles:
  fix bugs
  update code
  fix #123
  WIP
-->

## Description

<!-- What does this change do and why? Link related issues: Fixes #123 -->

## Type of change

<!-- Check the one that applies. -->

- [ ] Bug fix
- [ ] New feature
- [ ] Refactor
- [ ] Documentation
- [ ] CI / build / packaging

## How has this been tested?

<!-- Check what passed. Explain unchecked relevant items below. -->

- [ ] `cmake --preset default`
- [ ] `cmake --build --preset default`
- [ ] `ctest --preset default`
- [ ] `build/default/aria2-next --version`
- [ ] Packaging checks were run for packaging changes
- [ ] Manual testing completed, or not needed for this change

Unchecked checks:

<!-- Use "N/A: docs only", "N/A: no packaging changes", or explain the failure/risk. -->

## AI usage disclosure

<!-- Check the one that applies. Honest disclosure is expected. -->

- [ ] No AI tools were used
- [ ] AI tools assisted with drafting, refactoring, or boilerplate, and I reviewed every line
- [ ] Substantial portions were AI-generated, and I reviewed, tested, and can explain every change

AI model:

<!-- Required if AI was used. Use the exact model name. Generic names like ChatGPT or Claude are not enough. -->

## Checklist

- [ ] I kept this PR template complete
- [ ] I read `docs/CONTRIBUTING.md`
- [ ] This PR is focused on one concern
- [ ] The PR title uses Conventional Commits
- [ ] Documentation was updated for user-facing behavior, packaging, or workflow changes
- [ ] Tests were added or updated for risky logic changes, or this PR explains why tests are not needed
- [ ] Dependency, release, or runtime-linking changes update `packaging/dependencies.env` or packaging docs when applicable

## Release notes

<!-- One short user-facing line, or "none" if not user-facing. -->

Notes:
