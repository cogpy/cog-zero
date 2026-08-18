# agentzero-integration — Phase 9 Full-System Tests

Cross-module integration, performance benchmarks, and regression baselines
that exercise **all** Agent-Zero modules together on a shared AtomSpace.

## Coverage

| Suite | Target | Label | What it validates |
|-------|--------|-------|-------------------|
| `test_full_system.cpp` | `agentzero_system_integration` | `integration` | Core + perception + knowledge + planning + learning + communication + memory + tools |
| `test_system_benchmarks.cpp` | `agentzero_system_benchmarks` | `benchmark` | Routine decisions & queries under the Phase 9 **&lt; 100 ms** budget |
| `test_system_regression.cpp` | `agentzero_system_regression` | `regression` | Stable behavioural contracts (plan length, triple queries, lifecycle, …) |

## Build

From the repository root (uses in-tree AtomSpace shim when OpenCog is absent):

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_OPENCOG_MODULES=OFF
cmake --build build --target agentzero_system_integration_tests \
                      --target agentzero_system_benchmark_tests \
                      --target agentzero_system_regression_tests
```

## Run

```bash
cd build
ctest -R agentzero_system --output-on-failure
# or by label:
ctest --label-regex 'phase9' --output-on-failure
ctest --label-regex 'benchmark' --output-on-failure
ctest --label-regex 'regression' --output-on-failure
```

## Modules linked

- `agentzero-core`
- `agentzero-perception`
- `agentzero-knowledge`
- `agentzero-planning`
- `agentzero-learning`
- `agentzero-communication`
- `agentzero-memory`
- `agentzero-tools`
