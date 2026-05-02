# cog0 API Reference

*cog0 standalone library — v0.1.0*

---

## Overview

The `cog0` standalone library provides a self-contained C++ implementation of a cognitive agent loop.  It has **no external dependencies** beyond the C++17 standard library and POSIX threads.

The library consists of five subsystems exposed through a unified `Agent` facade:

| Subsystem | Header | Purpose |
|-----------|--------|---------|
| `Agent` | `cog0/Agent.h` | Top-level facade |
| `AtomStore` | `cog0/AtomStore.h` | Thread-safe typed hypergraph |
| `TaskManager` | `cog0/TaskManager.h` | Goal and task scheduling |
| `ReasoningEngine` | `cog0/ReasoningEngine.h` | Forward-chaining inference |
| `CognitiveLoop` | `cog0/CognitiveLoop.h` | Perception → action cycle |

For Python interoperability see the C API reference at the end of this document and `cog0/cog0_capi.h`.

---

## `Agent` — top-level facade

```cpp
#include "cog0/Agent.h"
namespace cog0 {
```

### `AgentConfig`

```cpp
struct AgentConfig {
    std::string name             = "cog0-agent";
    std::chrono::milliseconds cycleInterval{500};
    size_t      maxCycles        = 0;   // 0 = unlimited
    bool        enablePercept    = true;
    bool        enableReasoning  = true;
    bool        enablePlanning   = true;
    bool        enableAction     = true;
    bool        enableReflection = true;
    size_t      maxTasksPerCycle = 3;
    bool        verbose          = false;
};
```

| Field | Default | Description |
|-------|---------|-------------|
| `name` | `"cog0-agent"` | Human-readable agent identifier. |
| `cycleInterval` | 500 ms | Minimum pause between background cycles. |
| `maxCycles` | 0 | Stop background loop after this many cycles (0 = run forever). |
| `enablePercept` | true | Enable the perception phase. |
| `enableReasoning` | true | Enable the reasoning phase. |
| `enablePlanning` | true | Enable the planning phase. |
| `enableAction` | true | Enable the action phase. |
| `enableReflection` | true | Enable the reflection phase. |
| `maxTasksPerCycle` | 3 | Maximum tasks executed per cycle. |
| `verbose` | false | Enable DEBUG-level logging. |

### Constructor

```cpp
explicit Agent(AgentConfig cfg = {});
```

Creates and initialises all subsystems.  Three default inference rules are installed:
- **`goal-driven-assertion`** — asserts `AgentProperty:goal-driven` when any goal exists.
- **`high-salience-attention`** — asserts `AttentionFlag:high-salience` when any percept has STI > 0.8.
- **`inheritance-transitivity`** — closes transitive inheritance chains.

### Subsystem accessors

```cpp
AtomStore&       atomStore();
TaskManager&     taskManager();
ReasoningEngine& reasoningEngine();
CognitiveLoop&   cognitiveLoop();
```

### Convenience wrappers

```cpp
Goal::Ptr setGoal(const std::string& name,
                  const std::string& desc     = "",
                  double             priority = 1.0);
```

Add or replace a named goal.  Returns the `Goal` object.

```cpp
Task::Ptr scheduleTask(const std::string& name,
                       const std::string& desc   = "",
                       Priority           prio   = Priority::NORMAL,
                       std::function<bool()> action = nullptr);
```

Create and enqueue a task.  The optional `action` callback is invoked when the task is executed; returning `true` marks it complete, `false` marks it failed.

```cpp
void addPercept(const std::string& source,
                const std::string& content,
                double             salience = 0.5);
```

Thread-safe.  Enqueues a percept for processing in the next perception phase.

### Lifecycle

```cpp
void   start();            // Start background loop thread (non-blocking).
void   stop();             // Stop loop and join thread (blocking).
void   runCycles(size_t n);// Run exactly n synchronous cycles (blocking).
bool   isRunning() const;
```

`start()` / `stop()` are safe to call multiple times.  `start()` is a no-op if already running.  `stop()` is safe to call even if the loop stopped itself (via `maxCycles`).

### Diagnostics

```cpp
std::string statusReport() const;
```

Returns a multi-line human-readable report covering cycle count, AtomStore size, goals, tasks, and reasoning state.

---

## `AtomStore` — typed hypergraph

```cpp
#include "cog0/AtomStore.h"
namespace cog0 {
```

Thread-safe in-memory knowledge graph.  All methods acquire an internal mutex.

### Atom types

```cpp
enum class AtomType {
    CONCEPT, PREDICATE, VARIABLE, LIST,
    EVALUATION, INHERITANCE, SIMILARITY,
    IMPLICATION, EXECUTION, STATE, CUSTOM
};
```

### `TruthValue`

```cpp
struct TruthValue {
    double strength   = 1.0; // [0,1]
    double confidence = 1.0; // [0,1]
};
```

### `Atom`

```cpp
class Atom {
public:
    using Handle    = std::shared_ptr<Atom>;
    using HandleVec = std::vector<Handle>;

    AtomType           type()   const;
    const std::string& name()   const; // node name; empty for links
    const HandleVec&   out()    const; // outgoing set (empty for nodes)
    bool               isNode() const;
    bool               isLink() const;

    TruthValue tv()             const;
    void       setTV(TruthValue tv);

    double sti() const;  // short-term importance
    double lti() const;  // long-term importance
    void   setSTI(double v);
    void   setLTI(double v);

    size_t     id()    const;
    std::string toStr() const;
};

using Handle    = Atom::Handle;
using HandleVec = Atom::HandleVec;
```

### `AtomStore` API

```cpp
// Add or retrieve (deduplication via canonical keys)
Handle addNode(AtomType type, const std::string& name);
Handle addLink(AtomType type, HandleVec outgoing);

// Retrieve without creating (returns nullptr if absent)
Handle getNode(AtomType type, const std::string& name) const;
Handle getLink(AtomType type, const HandleVec& outgoing) const;

// Bulk queries
HandleVec getByType(AtomType type) const;
HandleVec getIncoming(const Handle& h) const;

// Mutation
void   remove(const Handle& h);
void   clear();
size_t size() const;
```

---

## `TaskManager` — goal and task scheduling

```cpp
#include "cog0/TaskManager.h"
namespace cog0 {
```

### Priority enum

```cpp
enum class Priority { CRITICAL = 0, HIGH, NORMAL, LOW, BACKGROUND };
```

### `Goal`

```cpp
struct Goal {
    using Ptr = std::shared_ptr<Goal>;
    size_t      id;
    std::string name, description;
    double      priority;
    bool        achieved;
    Handle      atom;            // representation in AtomStore
    std::vector<Task::Ptr> tasks;
};
```

### `Task`

```cpp
struct Task {
    using Ptr = std::shared_ptr<Task>;
    size_t      id;
    std::string name, description;
    Priority    priority;
    double      progress;        // 0.0 – 1.0
    bool        completed, failed;
    Handle      atom;
    std::vector<Ptr> subtasks;
    std::function<bool()> action;

    void addSubtask(Ptr sub);
    bool allSubtasksComplete() const;
};
```

### `TaskManager` API

```cpp
// Goals
Goal::Ptr setGoal(const std::string& name, const std::string& desc = "",
                  double priority = 1.0);
Goal::Ptr currentGoal() const;
const std::vector<Goal::Ptr>& goals() const;
bool achieveGoal(size_t goalId);

// Tasks
Task::Ptr createTask(const std::string& name,
                     const std::string& desc = "",
                     Priority prio = Priority::NORMAL,
                     std::function<bool()> action = nullptr);
void enqueue(Task::Ptr task);
void attachToGoal(size_t goalId, Task::Ptr task);

// Execution
bool   executeNext();           // Execute highest-priority pending task.
bool   executeAll();            // Execute all pending tasks.
size_t pendingCount() const;
std::vector<Task::Ptr> pendingTasks() const;
std::string statusReport() const;
```

---

## `ReasoningEngine` — forward-chaining inference

```cpp
#include "cog0/ReasoningEngine.h"
namespace cog0 {
```

### `InferenceRule`

```cpp
struct InferenceRule {
    using Ptr       = std::shared_ptr<InferenceRule>;
    using Condition = std::function<bool(const AtomStore&)>;
    using Action    = std::function<void(AtomStore&)>;

    std::string name;
    Condition   condition;
    Action      action;
    double      priority;   // higher = evaluated first
    size_t      fireCount;  // incremented each time rule fires
};
```

### `ReasoningEngine` API

```cpp
// Rule registration
void addRule(InferenceRule::Ptr rule);
void addRule(std::string name, Condition cond, Action act, double priority = 1.0);
void removeRule(const std::string& name);
const std::vector<InferenceRule::Ptr>& rules() const;

// Inference
std::vector<InferenceResult> runCycle();
size_t runForwardChaining(size_t maxCycles = 10);

// Queries
bool queryExists(AtomType t, const std::string& name) const;
bool queryInherits(const std::string& child, const std::string& parent) const;
bool queryEval(const std::string& pred, const std::string& arg,
               double threshold = 0.5) const;

std::string statusReport() const;
```

`runCycle()` evaluates all rules (sorted by descending priority) once and returns a result for each rule that fired.  `runForwardChaining()` repeats until no new rules fire or `maxCycles` passes are reached.

---

## `CognitiveLoop` — perception → action cycle

```cpp
#include "cog0/CognitiveLoop.h"
namespace cog0 {
```

### `PerceptInput`

```cpp
struct PerceptInput {
    std::string source;   // e.g. "camera-1"
    std::string modality; // e.g. "text", "numeric", "event"
    std::string content;  // raw content
    double      salience; // [0,1] attention weight
};
```

### `CycleStats`

```cpp
struct CycleStats {
    size_t cycleNumber;
    size_t perceptsAdded;
    size_t rulesFired;
    size_t tasksExecuted;
    std::chrono::milliseconds duration;
};
```

### `CognitiveLoop` API

```cpp
// Configuration (call before start())
void setCycleInterval(std::chrono::milliseconds ms);
void setMaxCycles(size_t n);            // 0 = unlimited
void setMaxTasksPerCycle(size_t n);
void enablePhase(bool perception, bool reasoning,
                 bool planning, bool action, bool reflection);

// Percept injection (thread-safe)
void addPercept(PerceptInput p);

// Hooks — called inside the loop; must be fast and non-throwing
using PerceptionHook = std::function<void(const PerceptInput&, AtomStore&)>;
using ReflectionHook = std::function<void(const CycleStats&)>;
void setPerceptionHook(PerceptionHook h);
void setReflectionHook(ReflectionHook h);

// Lifecycle
void   start();           // Launch background thread.
void   stop();            // Stop and join thread.
void   runSingleCycle();  // Run one synchronous cycle.
bool   isRunning() const;

// Statistics
size_t     cycleCount() const;
CycleStats lastStats()  const;
```

---

## C API (`cog0_capi.h`) — Python / FFI bridge

The C API exposes the `Agent` through an opaque handle type (`cog0_agent_t`), making it callable from Python (via `ctypes`), Cython, or any C-FFI.

```c
#include "cog0/cog0_capi.h"
```

### Lifecycle

```c
cog0_agent_t cog0_agent_create(const char* name,
                                int cycle_interval_ms,
                                size_t max_tasks_per_cycle);
void cog0_agent_free(cog0_agent_t agent);
```

### Goals & percepts

```c
int  cog0_agent_set_goal(cog0_agent_t, const char* name,
                          const char* desc, double priority);
void cog0_agent_add_percept(cog0_agent_t, const char* source,
                             const char* content, double salience);
```

### Loop control

```c
void   cog0_agent_run_cycles(cog0_agent_t, size_t n);
void   cog0_agent_start(cog0_agent_t);
void   cog0_agent_stop(cog0_agent_t);
int    cog0_agent_is_running(cog0_agent_t);
```

### Introspection

```c
size_t cog0_agent_cycle_count(cog0_agent_t);
size_t cog0_agent_atom_count(cog0_agent_t);
char*  cog0_agent_status_report(cog0_agent_t); // caller must free()
int    cog0_agent_has_concept(cog0_agent_t, const char* name);
void   cog0_agent_add_concept(cog0_agent_t, const char* name);
const char* cog0_version(void);
```

### Python ctypes example

```python
import ctypes, os

lib = ctypes.CDLL("libcog0.so")
lib.cog0_agent_create.restype  = ctypes.c_void_p
lib.cog0_agent_create.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_size_t]
lib.cog0_agent_free.argtypes   = [ctypes.c_void_p]
lib.cog0_agent_status_report.restype  = ctypes.c_char_p
lib.cog0_agent_status_report.argtypes = [ctypes.c_void_p]

agent = lib.cog0_agent_create(b"py-agent", 100, 5)
lib.cog0_agent_set_goal(agent, b"learn", b"Learn something", ctypes.c_double(0.9))
lib.cog0_agent_add_percept(agent, b"env", b"stimulus", ctypes.c_double(0.7))
lib.cog0_agent_run_cycles(agent, 3)
print(lib.cog0_agent_status_report(agent).decode())
lib.cog0_agent_free(agent)
```

---

## Performance targets

| Operation | Target | Notes |
|-----------|--------|-------|
| Single cognitive cycle | < 100 ms | All phases enabled, realistic state |
| AtomStore CRUD (1 000 ops) | < 1 s | Single-threaded |
| Rule evaluation (100 rules) | < 500 ms | Single `runCycle()` call |
| Task scheduling (200 tasks) | < 500 ms | `createTask` + `enqueue` + `executeAll` |
| 50 full cycles | < 5 s | Includes reasoning and reflection |

See `standalone/tests/test_benchmarks.cpp` for the automated benchmark suite.
