#!/usr/bin/env bash
# Build standalone Release packages (TGZ/ZIP, and DEB on Linux).
# Usage: scripts/package.sh [build-dir]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${ROOT}/build-package}"
VERSION="$(tr -d '[:space:]' < "${ROOT}/VERSION")"

echo "==> Packaging cog0 ${VERSION}"
echo "    source: ${ROOT}"
echo "    build:  ${BUILD_DIR}"

cmake -S "${ROOT}/standalone" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DCOGZERO_ENABLE_CPACK=ON

cmake --build "${BUILD_DIR}" --parallel

# Portable archives
cpack --config "${BUILD_DIR}/CPackConfig.cmake" -G TGZ
cpack --config "${BUILD_DIR}/CPackConfig.cmake" -G ZIP

if [[ "$(uname -s)" == "Linux" ]]; then
  cpack --config "${BUILD_DIR}/CPackConfig.cmake" -G DEB || true
fi

echo "==> Artifacts in ${BUILD_DIR}:"
ls -la "${BUILD_DIR}"/cog0-*.tar.gz "${BUILD_DIR}"/cog0-*.zip 2>/dev/null || \
  ls -la "${BUILD_DIR}"/*cog0* 2>/dev/null || true

# Checksums next to packages
(
  cd "${BUILD_DIR}"
  shopt -s nullglob
  files=(cog0-*.tar.gz cog0-*.zip cog0*.deb)
  if ((${#files[@]})); then
    sha256sum "${files[@]}" > SHA256SUMS
    echo "==> SHA256SUMS written"
    cat SHA256SUMS
  fi
)
