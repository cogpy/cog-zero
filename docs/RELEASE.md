# Release runbook

Operator guide for cutting a cog0 / cog-zero release.

## Prerequisites

1. `main` is green on [CI](../.github/workflows/ci.yml).
2. `VERSION` contains the new `X.Y.Z`.
3. `CHANGELOG.md` has a `## [X.Y.Z] - YYYY-MM-DD` section with release notes.
4. No secrets or credentials in the tree.

## Checklist

1. **Bump version**
   - Edit `VERSION`
   - Update `CHANGELOG.md` (`Unreleased` → new section)
2. **Validate packaging locally**
   ```bash
   ./scripts/packaging_smoke.sh
   ./scripts/package.sh
   ```
3. **Commit and tag**
   ```bash
   git commit -am "Release vX.Y.Z"
   git tag -a "vX.Y.Z" -m "cog0 vX.Y.Z"
   git push origin main --tags
   ```
4. **GitHub Actions** (`.github/workflows/release.yml`)
   - Validates tag == `VERSION` and CHANGELOG section exists
   - Builds standalone on Linux / macOS / Windows
   - Runs `cog0 --version` smoke
   - CPack TGZ/ZIP (+ DEB on Linux)
   - Publishes GitHub Release + `SHA256SUMS`
   - Builds/pushes `ghcr.io/cogpy/cog-zero` image tags
5. **Manual verification**
   - Download one archive per OS; run binary `--version`
   - `docker pull ghcr.io/cogpy/cog-zero:X.Y.Z && docker run --rm … --version`
6. **Python (optional for this release train)**
   - Build sdist/wheel from `agentzero-python-bridge`
   - Publish to TestPyPI first; PyPI after review

## Dry run

Use **workflow_dispatch** on `release.yml` with the version input matching
`VERSION`. Prefer a draft/pre-release tag workflow only after confirming the
job matrix is healthy.

## Permissions

- `contents: write` — create GitHub Release and upload assets
- `packages: write` — push to GHCR
- Do not embed deployment secrets in artifacts

## Failure modes

| Symptom | Fix |
|---------|-----|
| Version mismatch | Align tag, `VERSION`, and CHANGELOG heading |
| Missing CHANGELOG section | Add `## [X.Y.Z]` before tagging |
| Windows package empty | Ensure CPack ZIP generator and `cog0.exe` install rule |
| Docker push denied | Check `packages: write` and GHCR package visibility |
| `find_package(Cog0)` fails | Install Development component; set `CMAKE_PREFIX_PATH` |

## Out of scope (for now)

Homebrew, Snap, Conan, conda-forge, Windows MSI, Helm — see issue #36 phase 6.
