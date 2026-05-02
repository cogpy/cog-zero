# cog0 Deployment Guide

*cog0 standalone — v0.1.0*

---

## Prerequisites

| Requirement | Minimum version | Notes |
|-------------|-----------------|-------|
| C++ compiler | GCC 7 / Clang 5 / MSVC 2017 | C++17 required |
| CMake | 3.14 | Build system |
| POSIX threads | — | Available on Linux, macOS, WSL |
| Ninja *(optional)* | 1.9 | Faster builds |

> **No OpenCog installation required.**  The standalone build uses only the
> C++17 standard library and `<thread>`.

---

## Building from source

### 1. Clone and enter the standalone directory

```bash
git clone https://github.com/ReZorg/cog0.git
cd cog0/standalone
```

### 2. Configure

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=ON \
      ..
```

Available CMake options:

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | `Release` or `Debug` |
| `BUILD_TESTING` | `ON` | Build the test suite |

### 3. Build

```bash
cmake --build . --parallel
```

This produces:
- `libcog0.a` — static library
- `cog0` — interactive CLI binary
- `tests/cog0_tests` — test runner (when `BUILD_TESTING=ON`)

### 4. Run tests

```bash
ctest --output-on-failure
```

Expected output: **79 tests, 0 failures**.

### 5. Install (optional)

```bash
cmake --install . --prefix /usr/local
```

Installs:
- `<prefix>/bin/cog0`
- `<prefix>/lib/libcog0.a`
- `<prefix>/include/cog0/` (all public headers)

---

## Using the `cog0` CLI

### Interactive REPL

```bash
./cog0
```

```
cog0 v0.1.0 — Interactive Agent REPL
Type 'help' for a list of commands.

cog0> help
  goal <name> [description]   Set a goal
  task <name> [description]   Schedule a task
  percept <source> <content>  Inject a percept
  run [N]                     Run N cognitive cycles (default: 1)
  status                      Print agent status report
  atoms                       List all atoms in the store
  help                        Show this help
  quit / exit                 Exit

cog0> goal explore "Explore the environment"
cog0> percept sensor obstacle-ahead
cog0> run 3
cog0> status
```

### Script execution

```bash
./cog0 --script mission.cog0
```

`mission.cog0`:
```
goal survive Stay alive
goal explore Map the area
percept lidar clear-path
task move-forward Execute locomotion
run 10
status
```

### Inline evaluation

```bash
./cog0 --eval "goal test Run a quick test; percept src event; run 1; status"
```

Semicolons separate commands within a single `--eval` string.

### Demo mode

```bash
./cog0 --demo
```

Runs a built-in demonstration scenario and prints results.

---

## Embedding the library

### CMake integration

Add `cog0lib` as a dependency in your `CMakeLists.txt`:

```cmake
# Assuming cog0/standalone is added as a subdirectory or installed
find_package(cog0 REQUIRED)          # if installed
# or:
add_subdirectory(path/to/cog0/standalone)

target_link_libraries(my_app PRIVATE cog0lib)
target_include_directories(my_app PRIVATE path/to/cog0/standalone/include)
```

### Minimal C++ example

```cpp
#include "cog0/Agent.h"

int main() {
    cog0::AgentConfig cfg;
    cfg.name             = "my-agent";
    cfg.cycleInterval    = std::chrono::milliseconds(100);
    cfg.maxTasksPerCycle = 5;
    cog0::Agent agent(cfg);

    agent.setGoal("explore", "Explore the environment", 0.9);
    agent.addPercept("lidar", "obstacle-detected", 0.85);
    agent.reasoningEngine().addRule(
        "obstacle-response",
        [](const cog0::AtomStore& s) {
            return s.getNode(cog0::AtomType::CONCEPT,
                             "Percept:obstacle-detected") != nullptr;
        },
        [](cog0::AtomStore& s) {
            s.addNode(cog0::AtomType::CONCEPT, "Action:stop");
        },
        5.0
    );
    agent.runCycles(3);

    std::cout << agent.statusReport();
    return 0;
}
```

---

## Python interoperability

### Option A — ctypes (no build dependency)

Compile the standalone library as a **shared library** by adding `-DBUILD_SHARED_LIBS=ON` to CMake, then use Python's `ctypes`:

```bash
cmake -DBUILD_SHARED_LIBS=ON ..
cmake --build .
```

```python
import ctypes

lib = ctypes.CDLL("./libcog0.so")   # or libcog0.dylib on macOS

lib.cog0_agent_create.restype  = ctypes.c_void_p
lib.cog0_agent_create.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_size_t]
lib.cog0_agent_free.argtypes   = [ctypes.c_void_p]
lib.cog0_agent_run_cycles.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
lib.cog0_agent_status_report.restype  = ctypes.c_char_p
lib.cog0_agent_status_report.argtypes = [ctypes.c_void_p]

agent = lib.cog0_agent_create(b"py-agent", 100, 5)
lib.cog0_agent_set_goal(agent, b"learn", b"", ctypes.c_double(1.0))
lib.cog0_agent_add_percept(agent, b"src", b"event", ctypes.c_double(0.8))
lib.cog0_agent_run_cycles(agent, 5)
report = lib.cog0_agent_status_report(agent)
print(report.decode())
lib.cog0_agent_free(agent)
```

The full C API surface is defined in `standalone/include/cog0/cog0_capi.h`.

### Option B — Cython bindings (`agentzero-python-bridge/`)

The `agentzero-python-bridge/` module provides higher-level Cython bindings for the full OpenCog-integrated build (requires `cogutil`, `atomspace`, `cogserver`).

Build requirements for the Cython bridge:
```bash
pip install cython>=0.29
```

Build:
```bash
cd agentzero-python-bridge
pip install -e .
```

Usage:
```python
from opencog.agentzero import AgentZeroCore
from opencog.atomspace import AtomSpace

atomspace = AtomSpace()
agent = AgentZeroCore(atomspace)
agent.initialize()
agent.step()
print(agent.status)
```

---

## CI integration

The recommended CI workflow (see `.github/workflows/ci.yml`):

```yaml
- name: Configure standalone
  run: cmake -S standalone -B standalone/build -G Ninja
         -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

- name: Build
  run: cmake --build standalone/build --parallel

- name: Test
  run: ctest --test-dir standalone/build --output-on-failure --parallel
```

Supported CI platforms:
- Ubuntu 22.04 (GCC, Clang)
- Ubuntu 24.04 (GCC, Clang)
- macOS 13+ (Clang via Homebrew)

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| `fatal error: cog0/Agent.h: No such file` | Include path not set | Add `-I path/to/standalone/include` to compiler flags, or use the CMake target. |
| Linker error: undefined `cog0::*` | Library not linked | Add `-lcog0` and `-L path/to/lib`. |
| Tests hang or time out | Background thread not stopped | Always call `agent.stop()` or let the destructor run. |
| `std::terminate` on second `start()` | Old thread not joined | Call `agent.stop()` before a second `agent.start()`. Fixed in v0.1.0 (`CognitiveLoop::start()` now joins any previous thread). |
| High CPU with `cycleInterval=0` | No sleep between cycles | Set a non-zero `cycleInterval` (e.g., 100 ms) for production use. |
