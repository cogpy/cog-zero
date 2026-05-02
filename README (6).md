# agentzero-knowledge — Phase 3: Knowledge Representation & Reasoning

**Module:** `agentzero-knowledge`  
**Dependencies:** cogutil, atomspace, pln *(optional)*, ure *(optional)*, miner *(optional)*

---

## Overview

This module implements **Phase 3** of the Agent-Zero roadmap, providing a dedicated
library for knowledge representation and reasoning on top of OpenCog's AtomSpace.

It delivers four primary components:

| Component | Class | Description |
|-----------|-------|-------------|
| Knowledge Base | `KnowledgeBase` | Extended AtomSpace operations: bulk load, SPARQL-like queries |
| Pattern Discovery | `PatternDiscovery` | Unsupervised pattern mining over episode history |
| Concept Formation | `ConceptFormation` | Automatic concept creation and refinement |
| PLN Rule Library | `PLNRuleLibrary` | Curated PLN rules for common agent-reasoning patterns |

---

## Components

### KnowledgeBase

Extended AtomSpace operations for convenient knowledge management.

```cpp
#include <opencog/agentzero/knowledge/KnowledgeBase.h>
using namespace opencog::agentzero::knowledge;

KnowledgeBase kb(atomspace);
kb.initialize();

// Bulk load (subject, predicate, object) triples
kb.loadFromTriples({
    {"cat",  "isa",      "mammal"},
    {"eagle","isa",      "bird"},
    {"cat",  "has-part", "paw"},
});

// SPARQL-like query: find everything that isa mammal
QueryTriple p{"?x", "isa", "mammal"};
auto result = kb.query({p});
for (auto& binding : result.bindings)
    std::cout << binding["?x"]->get_name() << "\n";

// Query by atom type
auto concepts = kb.queryByType(CONCEPT_NODE);

// Load from file (whitespace-separated triples)
kb.loadFromFile("/path/to/knowledge.txt");
```

### PatternDiscovery

Unsupervised pattern mining over a corpus of recorded agent experiences.

```cpp
#include <opencog/agentzero/knowledge/PatternDiscovery.h>

PatternDiscovery pd(atomspace);
pd.initialize();

// Record episodes (each is a root atom grouping an experience)
pd.recordEpisode(root_handle, "episode-1");
// ... record more ...

// Mine frequent patterns
MiningConfig cfg;
cfg.min_support = 3;       // appears in at least 3 episodes
cfg.max_results = 20;
auto patterns = pd.minePatterns(cfg);

for (auto& p : patterns)
    std::cout << "freq=" << p.frequency << " " << p.description << "\n";

// Top-K by surprisingness
auto top5 = pd.getPatternsBySurprisingness(5);
```

### ConceptFormation

Automatic creation of `ConceptNode`s and `InheritanceLink`s from observed exemplars.

```cpp
#include <opencog/agentzero/knowledge/ConceptFormation.h>

ConceptFormation cf(atomspace);
cf.initialize();

// Register positive exemplars
cf.observeExemplar(atomspace->add_node(CONCEPT_NODE, "lion"),  "feline");
cf.observeExemplar(atomspace->add_node(CONCEPT_NODE, "tiger"), "feline");
cf.observeExemplar(atomspace->add_node(CONCEPT_NODE, "wolf"),  "canine");

// Form concepts (promotes candidates that pass coherence/novelty thresholds)
ConceptFormationConfig config;
config.min_exemplars = 2;
cf.formConcepts(config);

Handle feline = cf.getConceptHandle("feline");

// Build subsumption hierarchy
cf.buildConceptHierarchy();

// Merge / split
cf.mergeConcepts("feline", "canine", "carnivore");
```

### PLNRuleLibrary

Curated set of PLN/URE rules with an integrated forward/backward chainer.

Built-in rule categories:

| Category | Rules |
|----------|-------|
| deduction  | `deduction` (A→B, B→C ⊢ A→C), `modus-ponens` (A, A→B ⊢ B) |
| abduction  | `inversion` (A→B ⊢ B→A proxy), `abduction` (A→C, B→C ⊢ A→B) |
| analogy    | `analogy` (A~B, B→C ⊢ A→C) |
| causal     | `causal` (causes(C,E) ⊢ leads-to(C,E)) |
| goal       | `goal-pursuit` (goal(G), can-achieve(A,G) ⊢ pursue(A)) |

```cpp
#include <opencog/agentzero/knowledge/PLNRuleLibrary.h>

PLNRuleLibrary lib(atomspace);
lib.initialize();
lib.loadBuiltinRules();

// Forward chain from premises
auto results = lib.forwardChain({ab_handle, bc_handle}, /*max_steps=*/3);

// Compute PLN deduction truth value
double s_ac, c_ac;
PLNRuleLibrary::deductionTV(0.9, 0.8, 0.8, 0.7, s_ac, c_ac);
```

---

## Building

```bash
cmake -B build \
      -DBUILD_TESTING=ON \
      -DBUILD_EXAMPLES=ON \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The module is automatically included when `PLN_FOUND` and `URE_FOUND` are true
(see the top-level `CMakeLists.txt`).  If PLN/URE are not installed the build
will skip this module with a `STATUS` message.  All four components also compile
and run correctly *without* PLN/URE installed, using built-in fallback
implementations.

---

## Testing

```bash
cmake --build build --target test
# or
ctest --test-dir build -R "KnowledgeBase|PatternDiscovery|ConceptFormation"
```

---

## Example

See `examples/knowledge_demo.cpp` for a self-contained walkthrough of all four
components.

```bash
./build/agentzero-knowledge/examples/knowledge_demo
```

---

## Architecture

```
agentzero-knowledge/
├── include/opencog/agentzero/knowledge/
│   ├── KnowledgeBase.h        # Bulk load + SPARQL-like queries
│   ├── PatternDiscovery.h     # Episode mining
│   ├── ConceptFormation.h     # Automatic concept creation
│   └── PLNRuleLibrary.h       # PLN rule set + chaining
├── src/
│   ├── KnowledgeBase.cpp
│   ├── PatternDiscovery.cpp
│   ├── ConceptFormation.cpp
│   └── PLNRuleLibrary.cpp
├── tests/
│   ├── KnowledgeBaseUTest.cxxtest
│   ├── PatternDiscoveryUTest.cxxtest
│   └── ConceptFormationUTest.cxxtest
└── examples/
    └── knowledge_demo.cpp
```
