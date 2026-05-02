# Testing cog0 Standalone CLI

## Overview
cog0 is a C++ cognitive architecture agent with a CLI (REPL + scripted modes). It has no web UI — all testing is shell-based.

## Build
```bash
cd standalone
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```
The binary is at `standalone/build/cog0`.

## Test Suite
```bash
cd standalone/build
ctest -V
```
This runs 4 test suites:
- **Unit tests** (~147 tests): AtomStore, TaskManager, ReasoningEngine, CognitiveLoop
- **E2E tests** (~14 tests): Full agent scenarios filtered by `--filter e2e`
- **Integration tests** (~12 tests): Multi-subsystem cooperation filtered by `--filter integration`
- **Benchmarks** (~19 benchmarks): Performance measurements

All tests should pass with 0 failures. Total runtime is ~3-5 seconds.

## CLI Modes
- `./cog0 --demo` — Runs built-in demo scenario (goal + knowledge graph + tasks + 5 cycles + reasoning queries). Good smoke test.
- `./cog0 --script <file>` — Runs commands from a script file (one per line, `#` comments).
- `./cog0 --eval "<cmd>"` — Runs a single command.
- `./cog0 --batch --script <file>` — Suppresses output, prints JSON status at end.
- `./cog0` — Interactive REPL with readline.

Useful flags: `--no-color`, `--verbose`, `--name <agent-name>`.

## REPL Commands
| Command | Description |
|---------|-------------|
| `goal <name> [desc]` | Set a new goal |
| `goals` | List all goals |
| `task <name> [desc]` | Schedule a task (NORMAL priority) |
| `percept <text>` | Inject a text percept |
| `run [N]` | Run N cognitive cycles (default: 1) |
| `infer` | Run one forward-chaining inference pass |
| `status` | Print agent status (cycles, atom count, goals, tasks, rules) |
| `atoms` | List all atoms by type (uses getByType internally) |
| `save <file>` | Save AtomStore snapshot |
| `load <file>` | Load AtomStore snapshot |
| `rule <name> if-exists <concept> then-add <concept>` | Add an inference rule |
| `help` | Show help |
| `quit` / `exit` | Exit |

## Scripted Testing
Create `.cog` script files for reproducible tests:
```
# test_reflection.cog
goal test-reflection Check bounded growth
run 1
status
run 49
status
```
Run with: `./cog0 --script test_reflection.cog --no-color 2>&1`

## Key Testing Scenarios

### Type Index Verification
The `atoms` command calls `getByType()` for each AtomType. If the type index is broken, atoms won't appear. The demo's reasoning queries (`cogutil inherits OpenCog?`) also depend on `getByType()` finding INHERITANCE links.

### Reflection Cleanup
Run many cycles and check `AtomStore size` in `status` output. With cleanup working, atom count should stay bounded (not grow by 3 per cycle from Cycle:N / Duration:Nms / STATE link metadata).

### Planning Phase
The planning phase promotes tasks attached to goals. Note: the CLI `task` command does NOT attach tasks to goals (it only enqueues). To test planning promotion, use unit tests or write C++ code that calls `attachToGoal()`. The unit test suite covers this.

### Persistence
Test save/load round-trips. Use relative or absolute paths. The persistence format is line-based text. After `load`, the type index should be rebuilt — verify with `atoms` command.

## CI Matrix
CI runs on: Ubuntu 22.04/24.04 (GCC + Clang), Windows (MSVC), macOS 14 (AppleClang), plus ASan+UBSan sanitizer build. All 8 checks should pass.

## Devin Secrets Needed
None — cog0 is a standalone C++ project with no external service dependencies.
