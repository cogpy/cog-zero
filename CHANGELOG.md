# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.3.0] - 2026-08-20

### Added

- Unified product version via top-level `VERSION` file (0.3.0) consumed by
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
- `docs/VERSIONING.md`, `docs/PACKAGING.md`, and `docs/RELEASE.md`.
- CLI flag `cog0 --version` / `-V`.

### Changed

- Root project version aligned with standalone product version (was 0.1.0).
- `CogZeroConfig.cmake` now includes exported targets and a package version file.
- Agent-zero module installs export `CogZeroTargets` and ship public headers only
  for real OpenCog builds (shim builds remain test-only and are not packaged).

### License

- AGPL-3.0 — LICENSE is installed with Runtime packages.

[Unreleased]: https://github.com/cogpy/cog-zero/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/cogpy/cog-zero/releases/tag/v0.3.0
