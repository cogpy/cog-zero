#!/usr/bin/env bash
# Packaging smoke test:
#   configure → build → install to DESTDIR → run cog0 --version
#   → configure find_package consumer against the install prefix
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "${ROOT}/VERSION")"
BUILD_DIR="${COG0_SMOKE_BUILD:-${ROOT}/build-packaging-smoke}"
STAGE="${COG0_SMOKE_STAGE:-${ROOT}/build-packaging-stage}"
PREFIX_DIR="/usr/local"

echo "==> Packaging smoke (version ${VERSION})"
rm -rf "${BUILD_DIR}" "${STAGE}"
mkdir -p "${STAGE}"

cmake -S "${ROOT}/standalone" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DCOGZERO_ENABLE_CPACK=ON \
  -DCMAKE_INSTALL_PREFIX="${PREFIX_DIR}"

cmake --build "${BUILD_DIR}" --parallel
DESTDIR="${STAGE}" cmake --install "${BUILD_DIR}"

BIN="${STAGE}${PREFIX_DIR}/bin/cog0"
if [[ ! -x "${BIN}" && -x "${BIN}.exe" ]]; then
  BIN="${BIN}.exe"
fi
if [[ ! -e "${BIN}" ]]; then
  echo "ERROR: installed binary not found at ${BIN}" >&2
  exit 1
fi

echo "==> Runtime: ${BIN} --version"
OUT="$("${BIN}" --version)"
echo "    ${OUT}"
echo "${OUT}" | grep -q "${VERSION}" || {
  echo "ERROR: version output '${OUT}' does not contain ${VERSION}" >&2
  exit 1
}

echo "==> Runtime: ${BIN} --help"
"${BIN}" --help >/dev/null

# Headers + cmake package
INC="${STAGE}${PREFIX_DIR}/include/cog0"
CMAKE_DIR="${STAGE}${PREFIX_DIR}/lib/cmake/Cog0"
# Some distros use lib64
if [[ ! -d "${CMAKE_DIR}" ]]; then
  CMAKE_DIR="${STAGE}${PREFIX_DIR}/lib64/cmake/Cog0"
fi
test -f "${INC}/AtomStore.h" || { echo "ERROR: missing header ${INC}/AtomStore.h" >&2; exit 1; }
test -f "${INC}/cog0_capi.h" || { echo "ERROR: missing header ${INC}/cog0_capi.h" >&2; exit 1; }
test -d "${CMAKE_DIR}" || { echo "ERROR: missing CMake package at ${CMAKE_DIR}" >&2; exit 1; }

echo "==> find_package(Cog0) consumer"
CONSUMER_BUILD="${BUILD_DIR}-consumer"
rm -rf "${CONSUMER_BUILD}"
cmake -S "${ROOT}/cmake/tests/find_package_consumer" -B "${CONSUMER_BUILD}" \
  -DCMAKE_PREFIX_PATH="${STAGE}${PREFIX_DIR}" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${CONSUMER_BUILD}" --parallel

# Static cog0lib should link without needing the build tree.
"${CONSUMER_BUILD}/cog0_consumer" | grep -q ok

echo "==> CPack TGZ smoke"
(
  cd "${BUILD_DIR}"
  cpack --config "${BUILD_DIR}/CPackConfig.cmake" -G TGZ
)
shopt -s nullglob
TGZ=( "${BUILD_DIR}"/cog0-*.tar.gz )
if ((${#TGZ[@]} == 0)); then
  TGZ=( "${BUILD_DIR}"/*.tar.gz )
fi
# Also accept component-suffixed names from older CPack configs
if ((${#TGZ[@]} == 0)); then
  TGZ=( "${BUILD_DIR}"/cog0-*-Runtime.tar.gz "${BUILD_DIR}"/cog0-*-Development.tar.gz )
fi
test "${#TGZ[@]}" -ge 1 || { echo "ERROR: no TGZ produced in ${BUILD_DIR}" >&2; ls -la "${BUILD_DIR}" >&2; exit 1; }
echo "    produced: ${TGZ[*]}"

# Clean accidental cwd packages from prior runs
rm -f "${ROOT}"/cog0-*.tar.gz "${ROOT}"/cog0-*.zip 2>/dev/null || true

echo "==> Packaging smoke PASSED"
