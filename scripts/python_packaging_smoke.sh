#!/usr/bin/env bash
# Python packaging smoke (RT3 MVP):
#   build sdist + pure-Python wheel from agentzero-python-bridge
#   install into a fresh venv and import cog0 / print version
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "${ROOT}/VERSION")"
BRIDGE="${ROOT}/agentzero-python-bridge"
WORK="${COG0_PY_SMOKE_DIR:-${ROOT}/build-python-packaging-smoke}"

echo "==> Python packaging smoke (version ${VERSION})"
rm -rf "${WORK}"
mkdir -p "${WORK}"

python3 -m venv "${WORK}/venv"
# shellcheck disable=SC1091
source "${WORK}/venv/bin/activate"
python -m pip install -U pip setuptools wheel build >/dev/null

echo "==> python -m build (sdist + wheel)"
(
  cd "${BRIDGE}"
  rm -rf build dist *.egg-info cog0.egg-info
  python -m build --outdir "${WORK}/dist"
)

shopt -s nullglob
WHEELS=( "${WORK}/dist"/cog0-*.whl )
SDISTS=( "${WORK}/dist"/cog0-*.tar.gz )
test "${#WHEELS[@]}" -ge 1 || { echo "ERROR: no wheel produced" >&2; ls -la "${WORK}/dist" >&2; exit 1; }
test "${#SDISTS[@]}" -ge 1 || { echo "ERROR: no sdist produced" >&2; ls -la "${WORK}/dist" >&2; exit 1; }
echo "    wheel: ${WHEELS[*]}"
echo "    sdist: ${SDISTS[*]}"

echo "==> pip install wheel + import smoke"
python -m pip install --force-reinstall "${WHEELS[0]}" >/dev/null
OUT="$(python -c "import cog0; print(cog0.__version__)")"
echo "    cog0.__version__ = ${OUT}"
echo "${OUT}" | grep -q "${VERSION}" || {
  echo "ERROR: Python version '${OUT}' does not contain ${VERSION}" >&2
  exit 1
}

# Pure-Python package must import without libcog0_capi present.
python -c "from cog0 import Cog0LibraryNotFound, Cog0Error; print('imports-ok')"

deactivate
echo "==> Python packaging smoke PASSED"
