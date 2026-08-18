# agentzero-python-bridge — Phase 9 Python Interoperability

Python bindings for the standalone **cog0** agent runtime via the C API
(`include/cog0/cog0_capi.h`).

## Features

- **ctypes package** (`cog0`) — zero build-time Python deps
- **Optional Cython extension** (`cog0._cog0`) — built when Cython is installed
- Shared library target: `libcog0_capi.so` / `.dylib`
- No OpenCog packages required

## Build

From the repository root:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_STANDALONE=ON
cmake --build build --target cog0_capi -j
```

Optional Cython extension (if `pip install cython`):

```bash
cmake -S . -B build -DAGENTZERO_PYTHON_BRIDGE_BUILD_CYTHON=ON
cmake --build build --target _cog0 -j
```

## Use

```python
import os
os.environ["COG0_CAPI_LIB"] = "build/agentzero-python-bridge/libcog0_capi.so"

from cog0 import Agent

with Agent(name="demo", cycle_interval_ms=10) as agent:
    agent.set_goal("explore", "Explore the environment", priority=0.9)
    agent.add_percept("camera", "obstacle-ahead", salience=0.8)
    agent.run_cycles(5)
    print(agent.status_report())
    print("atoms:", agent.atom_count)
    print("cycles:", agent.cycle_count)
```

## Tests

```bash
ctest -R agentzero_python_bridge --output-on-failure
# or directly:
COG0_CAPI_LIB=build/agentzero-python-bridge/libcog0_capi.so \
  PYTHONPATH=agentzero-python-bridge \
  python3 agentzero-python-bridge/tests/test_cog0_bridge.py
```

## Layout

```
agentzero-python-bridge/
  CMakeLists.txt
  README.md
  cog0/
    __init__.py      # public API
    agent.py         # ctypes Agent
    exceptions.py
    _cog0.pyx        # optional Cython bindings
  tests/
    test_cog0_bridge.py
```
