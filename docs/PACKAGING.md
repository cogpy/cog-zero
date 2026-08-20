# Packaging

This document describes how to build install trees and release artifacts for
cog-zero. See also [VERSIONING.md](VERSIONING.md) and [RELEASE.md](RELEASE.md).

## Release targets

| ID | Target | Primary artifacts |
|----|--------|-------------------|
| RT1 | Standalone runtime | `cog0` binary, LICENSE, README |
| RT2 | Standalone SDK | `libcog0.a`, headers `include/cog0/`, `Cog0` CMake package, `cog0.pc` |
| RT3 | Python `cog0` | wheel / sdist (`agentzero-python-bridge`) |
| RT4 | OpenCog libraries | `libagentzero-*`, `libcog0` (OpenCog), `CogZero` CMake package |
| RT5 | Containers | `ghcr.io/cogpy/cog-zero` |
| RT6 | System packages | TGZ/ZIP always; DEB/RPM on Linux via CPack |
| RT7 | Metadata | CHANGELOG, SHA256SUMS |

**Standalone-first:** portable packages do not require OpenCog. OpenCog packages
are a second track and need system cogutil/atomspace.

## Install components

| Component | Contents |
|-----------|----------|
| `Runtime` | `cog0` executable, LICENSE/README |
| `Development` | static `cog0lib`, headers, CMake config, pkg-config |
| `Python` | `libcog0_capi` + Python package files |
| `OpenCog` | agentzero-* + OpenCog `libcog0` + CogZero cmake/pc |
| `Tools` | optional (not packaged by default) |

```bash
cmake --install build --component Runtime --prefix /usr/local
cmake --install build --component Development --prefix /usr/local
```

## Local standalone package

```bash
# Helper
./scripts/package.sh build-package

# Or manually
cmake -S standalone -B build-rel \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF
cmake --build build-rel -j
cpack --config build-rel/CPackConfig.cmake -G TGZ
```

Archive names look like:

```text
cog0-0.3.0-linux-x86_64.tar.gz
cog0-0.3.0-macos-arm64.tar.gz
cog0-0.3.0-windows-x86_64.zip
```

## Packaging smoke test

```bash
./scripts/packaging_smoke.sh
```

This configures a Release standalone build, installs into a DESTDIR, runs
`cog0 --version`, configures `cmake/tests/find_package_consumer` against the
install prefix, produces a TGZ via CPack, and on Linux also produces DEB
packages and verifies the runtime `.deb` extracts a working `cog0` binary.

## Python (MVP)

```bash
# Automated smoke (venv + build + install + import)
./scripts/python_packaging_smoke.sh

# Or manually
cd agentzero-python-bridge
python -m pip install build
python -m build
# Pure-Python wheel loads libcog0_capi from the environment / system path:
#   export COG0_CAPI_LIB=/path/to/libcog0_capi.so
python -m pip install dist/*.whl
python -c "import cog0; print(cog0.__version__)"
```

Binary wheels that bundle `libcog0_capi` are a follow-up (cibuildwheel).
Tag releases attach the pure-Python sdist/wheel to the GitHub Release.

## Containers

```bash
docker build -t cog0:local .
docker run --rm cog0:local --version
docker run --rm cog0:local --help
# Interactive / demo:
docker run --rm -it cog0:local --demo
```

Image registry: `ghcr.io/cogpy/cog-zero`.

Default entrypoint is `cog0` with CMD `--version` (non-interactive). Override the
command for demos or batch scripts. Monitoring (8080) and AgentService (50051)
ports are exposed but not started by default.

## OpenCog track

With system OpenCog installed:

```bash
cmake -S . -B build-opencog \
  -DBUILD_OPENCOG_MODULES=ON \
  -DBUILD_STANDALONE=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-opencog -j
cmake --install build-opencog --component OpenCog
```

Shim builds (`BUILD_OPENCOG_MODULES=OFF` module tests) are **not** release
artifacts and are excluded from install/export.

## License

All packages ship under **AGPL-3.0**. Include LICENSE in every archive. Network
deployments of MonitoringServer / AgentService must comply with AGPL source-offer
obligations.
