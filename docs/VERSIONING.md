# Versioning

cog-zero uses **Semantic Versioning** (`MAJOR.MINOR.PATCH`).

## Single source of truth

The top-level [`VERSION`](../VERSION) file is the only place that defines the
product version. It is consumed by:

| Consumer | How |
|----------|-----|
| Root CMake (`AgentZeroCpp`) | `include(CogZeroVersion)` before `project()` |
| Standalone `cog0` | same |
| Library `VERSION` / `SOVERSION` properties | `COGZERO_VERSION` / `COGZERO_SOVERSION` |
| Python package `cog0` | reads `VERSION` or generated `_version.py` |
| CPack | `CPACK_PACKAGE_VERSION` |
| Container labels / release workflow | tag `vX.Y.Z` must match `VERSION` |

Do **not** hard-code version strings in module `CMakeLists.txt` or
`__init__.py` except as a last-resort fallback.

## SOVERSION policy

- `COGZERO_SOVERSION` equals the **major** component only.
- Bump major (and thus SOVERSION) only on ABI-breaking changes to installed
  shared libraries (`libcog0`, `libcog0_capi`, `libagentzero-*`).
- Minor/patch releases keep the same SOVERSION.

## CMake package names

| Package | `find_package` | Targets | Track |
|---------|----------------|---------|--------|
| Standalone SDK | `Cog0` | `Cog0::cog0lib`, `Cog0::cog0` | RT1/RT2 |
| OpenCog stack | `CogZero` | `CogZero::cog0`, `CogZero::agentzero-*` | RT4 |

Package version files use `COMPATIBILITY SameMajorVersion`.

## Release tags

- Git tags: `vMAJOR.MINOR.PATCH` (example: `v3.1.0`).
- The release workflow fails if the tag version does not match `VERSION` or if
  `CHANGELOG.md` lacks a `## [X.Y.Z]` section.
- Pre-releases may use a hyphenated version only after the version scheme is
  extended; today `VERSION` is strict `X.Y.Z`.

## Current product version

See [`VERSION`](../VERSION). Standalone `cog0` is the user-facing product line;
historical module versions of `0.1.0` are superseded by this unified number.
