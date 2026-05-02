/*
 * examples/knowledge_demo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Demonstration of the Agent-Zero Knowledge Representation & Reasoning module.
 *
 * Shows:
 *  1. KnowledgeBase — bulk loading triples and SPARQL-like queries
 *  2. PatternDiscovery — mining frequent patterns from episode history
 *  3. ConceptFormation — automatic concept creation and hierarchy
 *  4. PLNRuleLibrary — forward chaining with built-in rules
 *
 * Build and run:
 *   cmake -B build -DBUILD_EXAMPLES=ON && cmake --build build
 *   ./build/agentzero-knowledge/examples/knowledge_demo
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include <opencog/agentzero/knowledge/KnowledgeBase.h>
#include <opencog/agentzero/knowledge/PatternDiscovery.h>
#include <opencog/agentzero/knowledge/ConceptFormation.h>
#include <opencog/agentzero/knowledge/PLNRuleLibrary.h>

using namespace opencog;
using namespace opencog::agentzero::knowledge;

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

static void section(const std::string& title)
{
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

// ---------------------------------------------------------------------------
// 1. KnowledgeBase demo
// ---------------------------------------------------------------------------

static void demo_knowledge_base(AtomSpacePtr atomspace)
{
    section("1. KnowledgeBase");

    KnowledgeBase kb(atomspace);
    kb.initialize();

    // --- Bulk load triples ---
    std::cout << "\nLoading biological taxonomy triples...\n";
    std::vector<std::tuple<std::string, std::string, std::string>> triples = {
        {"cat",    "isa",        "mammal"},
        {"dog",    "isa",        "mammal"},
        {"eagle",  "isa",        "bird"},
        {"sparrow","isa",        "bird"},
        {"mammal", "isa",        "animal"},
        {"bird",   "isa",        "animal"},
        {"cat",    "has-part",   "paw"},
        {"dog",    "has-part",   "paw"},
        {"eagle",  "has-part",   "wing"},
        {"sparrow","has-part",   "wing"},
    };

    auto load_result = kb.loadFromTriples(triples, "bio");
    std::cout << "  Loaded: " << load_result.atoms_loaded
              << "  Failed: " << load_result.atoms_failed << "\n";

    // --- SPARQL-like query: all birds ---
    std::cout << "\nQuery: ?x bio:isa bio:bird\n";
    QueryTriple pat;
    pat.subject   = "?x";
    pat.predicate = "bio:isa";
    pat.object    = "bio:bird";

    auto qresult = kb.query({pat});
    std::cout << "  Matches: " << qresult.total_matches << "\n";
    for (auto& binding : qresult.bindings) {
        for (auto& [var, handle] : binding) {
            if (handle->is_node())
                std::cout << "    " << var << " = " << handle->get_name() << "\n";
        }
    }

    // --- Query by name substring ---
    std::cout << "\nQuery by name fragment 'wing':\n";
    auto name_hits = kb.queryByName("wing");
    for (auto& h : name_hits)
        std::cout << "  " << h->get_type_name() << "(" << h->get_name() << ")\n";

    std::cout << "\n" << kb.getStatsSummary() << "\n";
}

// ---------------------------------------------------------------------------
// 2. PatternDiscovery demo
// ---------------------------------------------------------------------------

static void demo_pattern_discovery(AtomSpacePtr atomspace)
{
    section("2. PatternDiscovery");

    PatternDiscovery pd(atomspace);
    pd.initialize();

    // Record synthetic episodes: each contains a "percept" action "outcome"
    std::cout << "\nRecording 6 episodes...\n";
    Handle common = atomspace->add_node(CONCEPT_NODE, "reward-signal");
    for (size_t i = 0; i < 6; ++i) {
        Handle percept = atomspace->add_node(CONCEPT_NODE, "percept-" + std::to_string(i % 3));
        Handle action  = atomspace->add_node(CONCEPT_NODE, "action-" + std::to_string(i % 2));
        Handle root    = atomspace->add_link(AND_LINK, {common, percept, action});
        pd.recordEpisode(root, "ep-" + std::to_string(i),
                         {{"reward", std::to_string(i % 2 == 0 ? 1 : -1)}});
    }

    std::cout << "Episodes recorded: " << pd.episodeCount() << "\n";

    // Mine patterns
    MiningConfig cfg;
    cfg.min_support = 2;
    cfg.max_results = 5;
    auto patterns = pd.minePatterns(cfg);

    std::cout << "\nTop patterns (support >= 2):\n";
    for (auto& p : patterns) {
        std::cout << "  [freq=" << p.frequency
                  << " support=" << p.support
                  << " surprise=" << p.surprisingness << "] "
                  << p.description << "\n";
    }

    std::cout << "\n" << pd.getStatsSummary() << "\n";
}

// ---------------------------------------------------------------------------
// 3. ConceptFormation demo
// ---------------------------------------------------------------------------

static void demo_concept_formation(AtomSpacePtr atomspace)
{
    section("3. ConceptFormation");

    ConceptFormation cf(atomspace);
    cf.initialize();

    // Observe exemplars for three concepts
    std::cout << "\nObserving exemplars for 'feline', 'canine', 'avian'...\n";

    std::vector<std::string> feline_names = {"domestic-cat", "lion", "tiger", "leopard"};
    std::vector<std::string> canine_names = {"domestic-dog", "wolf", "fox"};
    std::vector<std::string> avian_names  = {"eagle", "sparrow", "parrot", "penguin"};

    for (auto& name : feline_names) {
        Handle h = atomspace->add_node(CONCEPT_NODE, name);
        cf.observeExemplar(h, "feline");
    }
    for (auto& name : canine_names) {
        Handle h = atomspace->add_node(CONCEPT_NODE, name);
        cf.observeExemplar(h, "canine");
    }
    for (auto& name : avian_names) {
        Handle h = atomspace->add_node(CONCEPT_NODE, name);
        cf.observeExemplar(h, "avian");
    }

    // Form concepts with permissive thresholds
    ConceptFormationConfig cfg;
    cfg.min_exemplars     = 2;
    cfg.min_coherence     = 0.0;
    cfg.min_coverage      = 0.0;
    cfg.novelty_threshold = 0.0;

    size_t promoted = cf.formConcepts(cfg);
    std::cout << "Concepts promoted: " << promoted << "\n";

    for (auto& label : {"feline", "canine", "avian"}) {
        Handle h = cf.getConceptHandle(label);
        if (h != Handle::UNDEFINED)
            std::cout << "  Concept '" << label << "' → " << h->get_name() << "\n";
    }

    // Build hierarchy
    size_t links_added = cf.buildConceptHierarchy();
    std::cout << "Hierarchy InheritanceLinks added: " << links_added << "\n";

    std::cout << "\n" << cf.getStatsSummary() << "\n";
}

// ---------------------------------------------------------------------------
// 4. PLNRuleLibrary demo
// ---------------------------------------------------------------------------

static void demo_pln_rules(AtomSpacePtr atomspace)
{
    section("4. PLNRuleLibrary");

    PLNRuleLibrary lib(atomspace);
    lib.initialize();

    // Load built-in rules
    size_t loaded = lib.loadBuiltinRules();
    std::cout << "\nBuilt-in rules loaded: " << loaded << "\n";

    // Print rule list
    for (auto& rule : lib.getRules()) {
        std::cout << "  [" << rule.category << "] "
                  << rule.name << ": " << rule.description << "\n";
    }

    // Demonstrate deduction TV formula
    std::cout << "\nDeduction TV: s(A→B)=0.9, c=0.8; s(B→C)=0.8, c=0.7\n";
    double s_ac, c_ac;
    PLNRuleLibrary::deductionTV(0.9, 0.8, 0.8, 0.7, s_ac, c_ac);
    std::cout << "  → s(A→C)=" << s_ac << "  c(A→C)=" << c_ac << "\n";

    // Demonstrate modus ponens TV formula
    std::cout << "\nModus Ponens TV: s(A)=0.9, c=0.8; s(A→B)=0.85, c=0.75\n";
    double s_b, c_b;
    PLNRuleLibrary::modusPonensTV(0.9, 0.8, 0.85, 0.75, s_b, c_b);
    std::cout << "  → s(B)=" << s_b << "  c(B)=" << c_b << "\n";

    // Forward chaining: A→B, B→C → A→C (deduction)
    std::cout << "\nForward chaining: A→B + B→C\n";
    Handle a = atomspace->add_node(CONCEPT_NODE, "demo-A");
    Handle b = atomspace->add_node(CONCEPT_NODE, "demo-B");
    Handle c = atomspace->add_node(CONCEPT_NODE, "demo-C");
    Handle ab = atomspace->add_link(IMPLICATION_LINK, {a, b});
    Handle bc = atomspace->add_link(IMPLICATION_LINK, {b, c});
    ab->setTruthValue(SimpleTruthValue::createTV(0.9, 0.8));
    bc->setTruthValue(SimpleTruthValue::createTV(0.8, 0.7));

    auto results = lib.forwardChain({ab, bc}, 2);
    std::cout << "  Derived " << results.size() << " conclusions:\n";
    for (auto& r : results) {
        if (r.success && r.conclusion) {
            std::cout << "    Rule='" << r.rule_used << "'"
                      << " s=" << r.strength << " c=" << r.confidence << "\n";
        }
    }

    std::cout << "\n" << lib.getStatsSummary() << "\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/)
{
    std::cout << "Agent-Zero Knowledge Representation & Reasoning Demo\n";
    std::cout << "Phase 3 — agentzero-knowledge module\n";

    // Shared AtomSpace for all demos
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();

    try {
        demo_knowledge_base(atomspace);
        demo_pattern_discovery(atomspace);
        demo_concept_formation(atomspace);
        demo_pln_rules(atomspace);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\nDemo complete.\n";
    return 0;
}
