# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [3.1.0] - 2026-08-20

### Added

- Unified product version via top-level `VERSION` file (3.1.0) consumed by
  standalone `cog0`, monorepo CMake, Python bridge, CPack, and container labels.
- CMake install components: `Runtime`, `Development`, `Python`, `OpenCog`.
- Standalone SDK install: public headers under `include/cog0/`, `Cog0Config.cmake`,
  `Cog0Targets.cmake`, `Cog0ConfigVersion.cmake`, and `cog0.pc`.
- Shared CPack module (`cmake/CogZeroCPack.cmake`) producing TGZ/ZIP (and DEB/RPM
  on Linux) for standalone Runtime + Development packages.
- Packaging smoke test (`scripts/packaging_smoke.sh`) and `find_package(Cog0)`
  consumer project under `cmake/tests/find_package_consumer/`.
- Release workflow (`.github/workflows/release.yml`) for tag-triggered multi-OS
  artifacts, SHA256SUMS, and GitHub Releases.
- Multi-stage `Dockerfile` for standalone runtime images on GHCR
  (`ghcr.io/cogpy/cog-zero`).
- Python packaging MVP: `agentzero-python-bridge/pyproject.toml` with version
  aligned to `VERSION`.
- Python packaging smoke (`scripts/python_packaging_smoke.sh`) and CI job
  building sdist + pure-Python wheel with import/version checks.
- Release workflow attaches Python sdist/wheel artifacts alongside native
  archives (RT3 MVP).
- Packaging smoke verifies Linux DEB packages via `dpkg-deb -x` and
  `cog0 --version`.
- ROADMAP Phase 15 documents packaging & release targets.
- `docs/VERSIONING.md`, `docs/PACKAGING.md`, and `docs/RELEASE.md`.
- CLI flag `cog0 --version` / `-V`.

### Changed

- Root project version aligned with standalone product version (was 0.1.0).
- Product version aligned with published GitHub release tags (`v3.1.0`).
- `CogZeroConfig.cmake` now includes exported targets and a package version file.
- Agent-zero module installs export `CogZeroTargets` and ship public headers only
  for real OpenCog builds (shim builds remain test-only and are not packaged).

### License

- AGPL-3.0 — LICENSE is installed with Runtime packages.

## [3.0.0] - 2026-08-18

### Added

- Phases 1–10 modular agentzero stack (core, perception, knowledge, planning,
  learning, communication, memory, tools, integration, distributed).
- Phase 14 production hardening: AgentService JSON-over-TCP API, MonitoringServer
  TLS, migration/serialize paths, and related tests.
- Multi-OS CI (Linux/macOS/Windows), fuzz and benchmark coverage, standalone CLI
  enhancements.

[Unreleased]: https://github.com/cogpy/cog-zero/compare/v3.1.0...HEAD
[3.1.0]: https://github.com/cogpy/cog-zero/releases/tag/v3.1.0
[3.0.0]: https://github.com/cogpy/cog-zero/releases/tag/v3.0.0
