# Agent-Zero CI Workflow Documentation

## Overview

This GitHub Actions workflow (`agent-zero-ci.yml`) provides automated continuous integration for the Agent-Zero adaptation of the OpenCog framework. It is designed based on the existing `cogci.yml` patterns and tailored specifically for the agent-zero components located in the `agents/` folder.

**Version**: 0.2.0
**Last Updated**: January 2026

## Workflow Features

### 1. **Multi-Stage Build Pipeline with Matrix Builds**

The workflow is organized into several dependent jobs that build the system incrementally across multiple configurations:

**Matrix Configurations**:
- **Operating Systems**: Ubuntu 22.04, Ubuntu 20.04
- **Compilers**: GCC, Clang

**Build Jobs**:
- **OpenCog Dependencies**: Builds foundational OpenCog components (cogutil, atomspace, cogserver)
- **Agent-Zero Core**: Builds the core agent-zero module
- **Agent-Zero Full System**: Builds all agent-zero modules with optional dependencies
- **Demonstrations**: Tests the agent-zero demonstration scenarios
- **Code Quality**: Performs static analysis and code quality checks
- **Code Coverage**: Generates lcov/gcovr coverage reports
- **Static Analysis**: Runs cppcheck and clang-tidy analysis
- **Performance Benchmarks**: Runs performance and regression tests
- **Docker Build**: Builds and publishes Docker images to GHCR
- **Build Summary**: Generates a comprehensive build report

### 2. **Trigger Conditions**

The workflow runs on:
- **Push events** to `main` or `develop` branches (when `agents/**` files change)
- **Pull requests** targeting `main` or `develop` branches (when `agents/**` files change)
- **Manual dispatch** via GitHub Actions UI with configurable options:
  - `enable_coverage`: Enable/disable code coverage analysis (default: true)
  - `enable_docker`: Enable/disable Docker image building (default: true)
  - `enable_benchmarks`: Enable/disable performance benchmarks (default: true)

### 3. **Caching Strategy**

The workflow uses multiple caching mechanisms to speed up builds:
- **ccache**: Compiler cache for faster C++ compilation
- **OpenCog dependencies**: Cached installation of cogutil, atomspace, and cogserver
- **Build artifacts**: Preserved for debugging and analysis

### 4. **Build Configuration**

Key environment variables:
```yaml
CMAKE_BUILD_TYPE: Release
BUILD_TESTING: ON
BUILD_EXAMPLES: ON
BUILD_DOCS: OFF
AGENT_ZERO_VERSION: 0.2.0

# Docker settings
DOCKER_REGISTRY: ghcr.io
DOCKER_IMAGE_NAME: ${{ github.repository }}/agent-zero
```

## Job Breakdown

### Job 1: `build-opencog-dependencies`

**Purpose**: Build and install the core OpenCog ecosystem components required by Agent-Zero.

**Steps**:
1. Install system dependencies (Boost, Guile, RocksDB, etc.)
2. Install Python dependencies (Cython, NumPy)
3. Build cogutil, atomspace, and cogserver
4. Cache the installed libraries for downstream jobs

**Dependencies**: None (foundation job)

**Artifacts**: OpenCog installation cache

---

### Job 2: `build-agent-zero-core`

**Purpose**: Build and test the core Agent-Zero module (`agentzero-core`).

**Steps**:
1. Restore OpenCog dependencies from cache
2. Configure and build `agents/cpp/agentzero-core`
3. Run unit tests with CTest
4. Install the core library

**Dependencies**: `build-opencog-dependencies`

**Artifacts**: Core build directory and test results

---

### Job 3: `build-agent-zero-full`

**Purpose**: Build the complete Agent-Zero system with all modules.

**Steps**:
1. Restore OpenCog dependencies from cache
2. Configure the full `agents/` directory
3. Build all agent-zero modules (perception, planning, memory, tools, etc.)
4. Run comprehensive test suite
5. Install the complete system

**Dependencies**: `build-agent-zero-core`

**Artifacts**: Full build directory and test results

**Note**: Some modules may be skipped if optional dependencies (PLN, URE, MOSES, etc.) are not available.

---

### Job 4: `test-demonstrations`

**Purpose**: Validate and run Agent-Zero demonstration scenarios.

**Steps**:
1. Restore OpenCog dependencies
2. Run `validate_demos.py` if present
3. Execute `run_all_demos.sh` to test demonstration scenarios
4. Upload demonstration logs and results

**Dependencies**: `build-agent-zero-full`

**Artifacts**: Demonstration logs and JSON results

**Note**: Failures are non-blocking (`continue-on-error: true`) as some demos may require runtime environments not available in CI.

---

### Job 5: `code-quality`

**Purpose**: Perform basic code quality checks.

**Steps**:
1. Check C++ code formatting with `clang-format`
2. Search for TODO/FIXME comments
3. Generate code statistics (file counts, lines of code)

**Dependencies**: None (runs in parallel)

**Artifacts**: None (output in logs)

---

### Job 6: `code-coverage`

**Purpose**: Generate code coverage reports using lcov and gcovr.

**Steps**:
1. Restore OpenCog dependencies from cache
2. Configure build with coverage flags (`--coverage`, `-fprofile-arcs`, `-ftest-coverage`)
3. Build Agent-Zero Core in Debug mode
4. Run tests to generate coverage data
5. Generate lcov HTML and summary reports
6. Generate gcovr XML and HTML reports
7. Upload coverage artifacts

**Dependencies**: `build-opencog-dependencies`

**Artifacts**: `coverage-reports` (coverage.info, coverage.html, coverage.xml)

**Configuration**: Can be disabled via workflow dispatch input `enable_coverage`

---

### Job 7: `static-analysis`

**Purpose**: Perform comprehensive static code analysis.

**Steps**:
1. Run cppcheck with all checks enabled
2. Run clang-tidy on sample files
3. Check code formatting consistency
4. Generate analysis summary in GitHub Actions

**Dependencies**: None (runs in parallel)

**Artifacts**: `static-analysis-reports` (cppcheck-report.xml)

**Tools Used**:
- **cppcheck**: Static analysis for C++ code
- **clang-tidy**: LLVM-based linter and static analyzer
- **clang-format**: Code style checker

---

### Job 8: `performance-benchmarks`

**Purpose**: Run performance benchmarks and regression testing.

**Steps**:
1. Build with Release optimizations (`-O3 -march=native`)
2. Run test suite with timing instrumentation
3. Profile memory usage
4. Store benchmark results as JSON
5. Generate performance summary

**Dependencies**: `build-agent-zero-full`

**Artifacts**: `benchmark-results` (results.json with timing and memory data)

**Configuration**: Can be disabled via workflow dispatch input `enable_benchmarks`

---

### Job 9: `docker-build`

**Purpose**: Build and publish Docker images to GitHub Container Registry.

**Steps**:
1. Set up Docker Buildx
2. Login to GitHub Container Registry
3. Generate multi-stage Dockerfile
4. Build optimized runtime image
5. Push to GHCR with semantic version tags
6. Generate image summary

**Dependencies**: `build-agent-zero-full`

**Artifacts**: Docker image published to `ghcr.io/<repo>/agent-zero`

**Configuration**: Can be disabled via workflow dispatch input `enable_docker`

**Image Tags**:
- `latest` (for default branch)
- `<version>` (e.g., `0.2.0`)
- `<branch>` (branch name)
- `<sha>` (commit SHA)

---

### Job 10: `build-summary`

**Purpose**: Generate a comprehensive build summary report.

**Steps**:
1. Aggregate status from all previous jobs
2. Generate GitHub Actions summary with build information
3. Determine overall build success/failure

**Dependencies**: All previous jobs

**Artifacts**: GitHub Actions summary page

---

## Installation

### 1. Copy the workflow file

Place `agent-zero-ci.yml` in your repository at:
```
.github/workflows/agent-zero-ci.yml
```

### 2. Verify repository structure

Ensure your repository has the following structure:
```
your-repo/
├── .github/
│   └── workflows/
│       └── agent-zero-ci.yml
├── agents/
│   ├── CMakeLists.txt
│   ├── cpp/
│   │   ├── agentzero-core/
│   │   ├── agentzero-perception/
│   │   ├── agentzero-planning/
│   │   └── ...
│   └── demonstrations/
├── cogutil/
├── atomspace/
└── cogserver/
```

### 3. Commit and push

```bash
git add .github/workflows/agent-zero-ci.yml
git commit -m "Add Agent-Zero CI workflow"
git push origin main
```

The workflow will automatically trigger on the next push or pull request.

---

## Customization

### Adjusting Build Options

Edit the `env` section in the workflow:

```yaml
env:
  CMAKE_BUILD_TYPE: Debug  # Change to Debug for debugging
  BUILD_TESTING: OFF       # Disable tests for faster builds
  BUILD_EXAMPLES: OFF      # Skip examples
  BUILD_DOCS: ON           # Enable documentation generation
```

### Adding Optional Dependencies

To build with optional OpenCog components (PLN, URE, MOSES), add installation steps in the `build-opencog-dependencies` job:

```yaml
- name: Build and install PLN
  run: |
    cd pln
    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=${{ env.CMAKE_BUILD_TYPE }}
    make -j$(nproc)
    sudo make install
    sudo ldconfig
```

### Modifying Trigger Paths

To trigger on changes to additional files:

```yaml
on:
  push:
    branches: [main, develop]
    paths:
      - 'agents/**'
      - 'cogutil/**'           # Add this
      - 'atomspace/**'         # Add this
      - '.github/workflows/agent-zero-ci.yml'
```

### Enabling Self-Healing (Advanced)

To add self-healing capabilities similar to `cogci.yml`, add a job:

```yaml
self-healing-demo:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - name: Setup Self-Healing Environment
      run: |
        chmod +x scripts/auto_fix.py
        mkdir -p ci_artifacts
    - name: Test with Auto-Fix
      run: |
        python3 scripts/auto_fix.py \
          --build-cmd "cd agents/build && make" \
          --max-attempts 3
```

---

## Troubleshooting

### Build Failures

**Issue**: OpenCog dependencies not found

**Solution**: Ensure the cache is properly restored. Check the `Restore OpenCog dependencies cache` step logs.

**Issue**: Boost version mismatch

**Solution**: Update the Boost version in `agents/cpp/CMakeLists.txt`:
```cmake
set(MIN_BOOST 1.71)  # Adjust as needed
```

### Cache Issues

**Issue**: Cache not restoring properly

**Solution**: Clear the cache by changing the cache key:
```yaml
key: opencog-deps-v2-${{ runner.os }}-${{ github.sha }}
```

### Test Failures

**Issue**: Tests fail in CI but pass locally

**Solution**: Check for environment-specific issues. Add debugging output:
```yaml
- name: Debug environment
  run: |
    echo "PATH: $PATH"
    echo "LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
    pkg-config --list-all | grep opencog
```

---

## Performance Optimization

### 1. **Parallel Builds**

The workflow uses `make -j$(nproc)` to utilize all available CPU cores.

### 2. **ccache Integration**

Compiler cache reduces rebuild times by ~50-70% for incremental changes.

### 3. **Dependency Caching**

OpenCog dependencies are cached and restored, avoiding full rebuilds on every run.

### 4. **Selective Triggering**

The workflow only runs when files in `agents/**` are modified, reducing unnecessary builds.

---

## Integration with Existing Workflows

This workflow is designed to complement the existing `cogci.yml` workflow. Key differences:

| Feature | cogci.yml | agent-zero-ci.yml |
|---------|-----------|-------------------|
| **Scope** | Full OpenCog ecosystem | Agent-Zero modules only |
| **Trigger** | All changes | `agents/**` changes only |
| **Self-Healing** | Yes | Optional (can be added) |
| **Dependencies** | Builds all components | Reuses cached OpenCog deps |
| **Focus** | System-wide integration | Agent-Zero specific testing |

---

## Continuous Improvement

### Implemented Enhancements (v0.2.0)

All recommended enhancements have been implemented:

1. **Matrix Builds**: Testing across Ubuntu 22.04/20.04 with GCC and Clang
2. **Performance Benchmarks**: Performance and regression testing with JSON output
3. **Code Coverage**: Integrated lcov and gcovr coverage reporting
4. **Static Analysis**: cppcheck and clang-tidy integration
5. **Docker Integration**: Multi-stage Docker builds with GHCR publishing

### Matrix Build Configuration

```yaml
build-agent-zero-core:
  strategy:
    fail-fast: false
    matrix:
      os: [ubuntu-22.04, ubuntu-20.04]
      compiler: [gcc, clang]
      include:
        - compiler: gcc
          cc: gcc
          cxx: g++
        - compiler: clang
          cc: clang
          cxx: clang++
  runs-on: ${{ matrix.os }}
  # ... rest of job
```

### Future Enhancements

1. **ARM64 Support**: Add matrix builds for ARM64 architecture
2. **Sanitizer Builds**: Add ASan/UBSan/TSan configurations
3. **Dependency Updates**: Automated dependency version checking
4. **Benchmark Regression**: Automated performance regression detection
5. **Coverage Thresholds**: Enforce minimum code coverage requirements

---

## Support and Contributions

For issues or questions about this workflow:

1. Check the [GitHub Actions logs](https://github.com/your-org/your-repo/actions)
2. Review the [Agent-Zero documentation](../agents/README.md)
3. Consult the [OpenCog build system guide](../agents/cpp/BUILD_SYSTEM.md)
4. Open an issue in the repository

---

## License

This workflow configuration is part of the OpenCog Agent-Zero project and follows the same license terms as the parent project.

---

## Changelog

### Version 0.2.0 (January 2026)
- **Matrix Builds**: Added multi-OS (Ubuntu 22.04/20.04) and multi-compiler (GCC/Clang) support
- **Code Coverage**: Integrated lcov and gcovr for comprehensive coverage reporting
- **Static Analysis**: Added cppcheck and clang-tidy integration
- **Performance Benchmarks**: Added performance testing with JSON output and regression tracking
- **Docker Integration**: Multi-stage Docker builds with GHCR publishing
- **Workflow Dispatch**: Added configurable inputs for coverage, Docker, and benchmarks
- **Enhanced Summary**: Matrix build status and artifact overview in build summary
- Bumped Agent-Zero version to 0.2.0

### Version 1.0.0 (Initial Release)
- Multi-stage build pipeline
- OpenCog dependency caching
- Agent-Zero core and full system builds
- Demonstration testing
- Code quality checks
- Build summary reporting
