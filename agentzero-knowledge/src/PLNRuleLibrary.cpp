/*
 * opencog/agentzero/knowledge/PLNRuleLibrary.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * PLNRuleLibrary Implementation
 * Part of Agent-Zero Knowledge Representation & Reasoning module
 * Part of the AGENT-ZERO-GENESIS project - Phase 3
 */

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/knowledge/PLNRuleLibrary.h"

using namespace opencog;
using namespace opencog::agentzero::knowledge;

// ---------------------------------------------------------------------------
// PLN Truth-Value Formulas (static, no AtomSpace required)
// ---------------------------------------------------------------------------

void PLNRuleLibrary::deductionTV(double strength_ab, double conf_ab,
                                   double strength_bc, double conf_bc,
                                   double& strength_ac, double& conf_ac)
{
    // Standard PLN deduction: s(A→C) ≈ s(A→B) * s(B→C)
    strength_ac = strength_ab * strength_bc;
    // Confidence: harmonic-mean-based formula
    conf_ac = (conf_ab > 0.0 && conf_bc > 0.0)
        ? (conf_ab * conf_bc) / (conf_ab + conf_bc - conf_ab * conf_bc)
        : 0.0;
    strength_ac = std::max(0.0, std::min(1.0, strength_ac));
    conf_ac     = std::max(0.0, std::min(1.0, conf_ac));
}

void PLNRuleLibrary::modusPonensTV(double strength_a, double conf_a,
                                    double strength_ab, double conf_ab,
                                    double& strength_b, double& conf_b)
{
    // s(B) ≈ s(A) * s(A→B)
    strength_b = strength_a * strength_ab;
    conf_b = (conf_a > 0.0 && conf_ab > 0.0)
        ? (conf_a * conf_ab) / (conf_a + conf_ab - conf_a * conf_ab)
        : 0.0;
    strength_b = std::max(0.0, std::min(1.0, strength_b));
    conf_b     = std::max(0.0, std::min(1.0, conf_b));
}

void PLNRuleLibrary::inversionTV(double strength_ab, double conf_ab,
                                  double strength_a,  double conf_a,
                                  double strength_b,  double conf_b,
                                  double& strength_ba, double& conf_ba)
{
    // Bayes inversion: s(B→A) = s(A|B) = s(A→B)*s(A) / s(B)
    if (strength_b <= 0.0) {
        strength_ba = 0.0;
        conf_ba = 0.0;
        return;
    }
    strength_ba = (strength_ab * strength_a) / strength_b;
    strength_ba = std::max(0.0, std::min(1.0, strength_ba));
    conf_ba = std::min({conf_ab, conf_a, conf_b});
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

PLNRuleLibrary::PLNRuleLibrary(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    if (!_atomspace)
        throw std::runtime_error("PLNRuleLibrary requires a valid AtomSpace");
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool PLNRuleLibrary::initialize()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_initialized) return true;
    logger().info() << "[PLNRuleLibrary] Initializing";
    _initialized = true;
    return true;
}

bool PLNRuleLibrary::shutdown()
{
    std::lock_guard<std::mutex> lock(_mutex);
    logger().info() << "[PLNRuleLibrary] Shutting down";
    _initialized = false;
    return true;
}

// ---------------------------------------------------------------------------
// Rule atom builders
// ---------------------------------------------------------------------------

Handle PLNRuleLibrary::buildDeductionRule()
{
    // Represent the deduction schema as an ImplicationLink in the AtomSpace:
    //   (Implication
    //     (And (Implication $A $B) (Implication $B $C))
    //     (Implication $A $C))
    Handle va = _atomspace->add_node(VARIABLE_NODE, "$A");
    Handle vb = _atomspace->add_node(VARIABLE_NODE, "$B");
    Handle vc = _atomspace->add_node(VARIABLE_NODE, "$C");
    Handle ab = _atomspace->add_link(IMPLICATION_LINK, {va, vb});
    Handle bc = _atomspace->add_link(IMPLICATION_LINK, {vb, vc});
    Handle ac = _atomspace->add_link(IMPLICATION_LINK, {va, vc});
    Handle ant = _atomspace->add_link(AND_LINK, {ab, bc});
    return _atomspace->add_link(IMPLICATION_LINK, {ant, ac});
}

Handle PLNRuleLibrary::buildModusPonensRule()
{
    // (Implication (And $A (Implication $A $B)) $B)
    Handle va = _atomspace->add_node(VARIABLE_NODE, "$A");
    Handle vb = _atomspace->add_node(VARIABLE_NODE, "$B");
    Handle ab = _atomspace->add_link(IMPLICATION_LINK, {va, vb});
    Handle ant = _atomspace->add_link(AND_LINK, {va, ab});
    return _atomspace->add_link(IMPLICATION_LINK, {ant, vb});
}

Handle PLNRuleLibrary::buildInversionRule()
{
    // (Implication (Implication $A $B) (Implication $B $A))
    Handle va = _atomspace->add_node(VARIABLE_NODE, "$A");
    Handle vb = _atomspace->add_node(VARIABLE_NODE, "$B");
    Handle ab = _atomspace->add_link(IMPLICATION_LINK, {va, vb});
    Handle ba = _atomspace->add_link(IMPLICATION_LINK, {vb, va});
    return _atomspace->add_link(IMPLICATION_LINK, {ab, ba});
}

Handle PLNRuleLibrary::buildAbductionRule()
{
    // (Implication (And (Implication $A $C) (Implication $B $C)) (Implication $A $B))
    Handle va = _atomspace->add_node(VARIABLE_NODE, "$A");
    Handle vb = _atomspace->add_node(VARIABLE_NODE, "$B");
    Handle vc = _atomspace->add_node(VARIABLE_NODE, "$C");
    Handle ac = _atomspace->add_link(IMPLICATION_LINK, {va, vc});
    Handle bc = _atomspace->add_link(IMPLICATION_LINK, {vb, vc});
    Handle ab = _atomspace->add_link(IMPLICATION_LINK, {va, vb});
    Handle ant = _atomspace->add_link(AND_LINK, {ac, bc});
    return _atomspace->add_link(IMPLICATION_LINK, {ant, ab});
}

Handle PLNRuleLibrary::buildAnalogyRule()
{
    // (Implication (And (SimilarityLink $A $B) (Implication $B $C)) (Implication $A $C))
    Handle va = _atomspace->add_node(VARIABLE_NODE, "$A");
    Handle vb = _atomspace->add_node(VARIABLE_NODE, "$B");
    Handle vc = _atomspace->add_node(VARIABLE_NODE, "$C");
    Handle sim_ab = _atomspace->add_link(SIMILARITY_LINK, {va, vb});
    Handle bc    = _atomspace->add_link(IMPLICATION_LINK, {vb, vc});
    Handle ac    = _atomspace->add_link(IMPLICATION_LINK, {va, vc});
    Handle ant   = _atomspace->add_link(AND_LINK, {sim_ab, bc});
    return _atomspace->add_link(IMPLICATION_LINK, {ant, ac});
}

Handle PLNRuleLibrary::buildCausalRule()
{
    // (Implication (EvaluationLink (PredicateNode "causes") (ListLink $C $E))
    //              (EvaluationLink (PredicateNode "leads-to") (ListLink $C $E)))
    Handle vc = _atomspace->add_node(VARIABLE_NODE, "$Cause");
    Handle ve = _atomspace->add_node(VARIABLE_NODE, "$Effect");
    Handle causes_pred  = _atomspace->add_node(PREDICATE_NODE, "causes");
    Handle leads_pred   = _atomspace->add_node(PREDICATE_NODE, "leads-to");
    Handle list_ce = _atomspace->add_link(LIST_LINK, {vc, ve});
    Handle ant = _atomspace->add_link(EVALUATION_LINK, {causes_pred, list_ce});
    Handle cons = _atomspace->add_link(EVALUATION_LINK, {leads_pred, list_ce});
    return _atomspace->add_link(IMPLICATION_LINK, {ant, cons});
}

Handle PLNRuleLibrary::buildGoalPursuitRule()
{
    // (Implication
    //   (And (EvaluationLink (PredicateNode "goal") (ListLink $G))
    //        (EvaluationLink (PredicateNode "can-achieve") (ListLink $A $G)))
    //   (EvaluationLink (PredicateNode "pursue-action") (ListLink $A)))
    Handle vg = _atomspace->add_node(VARIABLE_NODE, "$Goal");
    Handle va = _atomspace->add_node(VARIABLE_NODE, "$Action");
    Handle goal_p    = _atomspace->add_node(PREDICATE_NODE, "goal");
    Handle achieve_p = _atomspace->add_node(PREDICATE_NODE, "can-achieve");
    Handle pursue_p  = _atomspace->add_node(PREDICATE_NODE, "pursue-action");

    Handle list_g  = _atomspace->add_link(LIST_LINK, {vg});
    Handle list_ag = _atomspace->add_link(LIST_LINK, {va, vg});
    Handle list_a  = _atomspace->add_link(LIST_LINK, {va});

    Handle ant1 = _atomspace->add_link(EVALUATION_LINK, {goal_p,    list_g});
    Handle ant2 = _atomspace->add_link(EVALUATION_LINK, {achieve_p, list_ag});
    Handle cons = _atomspace->add_link(EVALUATION_LINK, {pursue_p,  list_a});
    Handle ant  = _atomspace->add_link(AND_LINK, {ant1, ant2});
    return _atomspace->add_link(IMPLICATION_LINK, {ant, cons});
}

// ---------------------------------------------------------------------------
// Load built-in rules
// ---------------------------------------------------------------------------

size_t PLNRuleLibrary::loadBuiltinRules()
{
    std::lock_guard<std::mutex> lock(_mutex);
    size_t loaded = 0;

    struct BuiltinDef {
        std::string name;
        std::string category;
        std::string description;
        Handle (PLNRuleLibrary::*builder)();
    };

    std::vector<BuiltinDef> defs = {
        {"deduction",       "deduction",  "A→B, B→C ⊢ A→C (transitivity)",      &PLNRuleLibrary::buildDeductionRule},
        {"modus-ponens",    "deduction",  "A, A→B ⊢ B",                          &PLNRuleLibrary::buildModusPonensRule},
        {"inversion",       "abduction",  "A→B ⊢ B→A (Bayes inversion proxy)",   &PLNRuleLibrary::buildInversionRule},
        {"abduction",       "abduction",  "A→C, B→C ⊢ A→B (common effect)",      &PLNRuleLibrary::buildAbductionRule},
        {"analogy",         "analogy",    "A~B, B→C ⊢ A→C (similarity transfer)",&PLNRuleLibrary::buildAnalogyRule},
        {"causal",          "causal",     "causes(C,E) ⊢ leads-to(C,E)",         &PLNRuleLibrary::buildCausalRule},
        {"goal-pursuit",    "goal",       "goal(G), can-achieve(A,G) ⊢ pursue(A)",&PLNRuleLibrary::buildGoalPursuitRule},
    };

    for (auto& def : defs) {
        if (_rules.count(def.name)) continue; // already registered
        try {
            Handle h = (this->*(def.builder))();
            PLNRule rule;
            rule.name = def.name;
            rule.category = def.category;
            rule.rule_atom = h;
            rule.description = def.description;
            rule.active = true;
            _rules[def.name] = std::move(rule);
            _category_index[def.category].insert(def.name);
            ++loaded;
        } catch (const std::exception& e) {
            logger().warn() << "[PLNRuleLibrary] Failed to build rule '"
                            << def.name << "': " << e.what();
        }
    }

    logger().info() << "[PLNRuleLibrary] Loaded " << loaded << " built-in rules";
    return loaded;
}

// ---------------------------------------------------------------------------
// Rule registration
// ---------------------------------------------------------------------------

bool PLNRuleLibrary::registerRule(const Handle& rule_atom,
                                   const std::string& name,
                                   const std::string& category,
                                   const std::string& description)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!rule_atom) return false;
    PLNRule rule;
    rule.name = name;
    rule.category = category;
    rule.rule_atom = rule_atom;
    rule.description = description;
    rule.active = true;
    _rules[name] = std::move(rule);
    _category_index[category].insert(name);
    return true;
}

bool PLNRuleLibrary::unregisterRule(const std::string& name)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _rules.find(name);
    if (it == _rules.end()) return false;
    _category_index[it->second.category].erase(name);
    _rules.erase(it);
    return true;
}

bool PLNRuleLibrary::setRuleActive(const std::string& name, bool active)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _rules.find(name);
    if (it == _rules.end()) return false;
    it->second.active = active;
    return true;
}

std::vector<PLNRule> PLNRuleLibrary::getRules() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<PLNRule> result;
    result.reserve(_rules.size());
    for (auto& [name, rule] : _rules)
        result.push_back(rule);
    return result;
}

std::vector<PLNRule> PLNRuleLibrary::getRulesByCategory(
    const std::string& category) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<PLNRule> result;
    auto it = _category_index.find(category);
    if (it == _category_index.end()) return result;
    for (auto& name : it->second) {
        auto rit = _rules.find(name);
        if (rit != _rules.end()) result.push_back(rit->second);
    }
    return result;
}

std::optional<PLNRule> PLNRuleLibrary::findRule(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _rules.find(name);
    if (it == _rules.end()) return std::nullopt;
    return it->second;
}

// ---------------------------------------------------------------------------
// Inference helpers
// ---------------------------------------------------------------------------

Handle PLNRuleLibrary::applyDeduction(const Handle& ab, const Handle& bc) const
{
    // ab = ImplicationLink(A, B), bc = ImplicationLink(B, C)
    // Check both are ImplicationLinks with arity 2
    if (!ab || !bc) return Handle::UNDEFINED;
    if (ab->get_type() != IMPLICATION_LINK || bc->get_type() != IMPLICATION_LINK)
        return Handle::UNDEFINED;
    if (ab->getArity() < 2 || bc->getArity() < 2) return Handle::UNDEFINED;

    Handle a = ab->getOutgoingAtom(0);
    Handle b_from_ab = ab->getOutgoingAtom(1);
    Handle b_from_bc = bc->getOutgoingAtom(0);
    Handle c = bc->getOutgoingAtom(1);

    if (b_from_ab != b_from_bc) return Handle::UNDEFINED;

    // Compute truth value
    TruthValuePtr tv_ab = ab->getTruthValue();
    TruthValuePtr tv_bc = bc->getTruthValue();
    double s_ab = tv_ab ? tv_ab->get_mean() : 0.5;
    double c_ab = tv_ab ? tv_ab->get_confidence() : 0.1;
    double s_bc = tv_bc ? tv_bc->get_mean() : 0.5;
    double c_bc = tv_bc ? tv_bc->get_confidence() : 0.1;

    double s_ac, c_ac;
    deductionTV(s_ab, c_ab, s_bc, c_bc, s_ac, c_ac);

    Handle ac = _atomspace->add_link(IMPLICATION_LINK, {a, c});
    ac->setTruthValue(SimpleTruthValue::createTV(s_ac, c_ac));
    return ac;
}

Handle PLNRuleLibrary::applyModusPonens(const Handle& a, const Handle& ab) const
{
    if (!a || !ab) return Handle::UNDEFINED;
    if (ab->get_type() != IMPLICATION_LINK || ab->getArity() < 2)
        return Handle::UNDEFINED;

    Handle a_from_ab = ab->getOutgoingAtom(0);
    if (a_from_ab != a) return Handle::UNDEFINED;

    Handle b = ab->getOutgoingAtom(1);

    TruthValuePtr tv_a  = a->getTruthValue();
    TruthValuePtr tv_ab = ab->getTruthValue();
    double s_a = tv_a ? tv_a->get_mean() : 0.5;
    double c_a = tv_a ? tv_a->get_confidence() : 0.1;
    double s_ab_v = tv_ab ? tv_ab->get_mean() : 0.5;
    double c_ab_v = tv_ab ? tv_ab->get_confidence() : 0.1;

    double s_b, c_b;
    modusPonensTV(s_a, c_a, s_ab_v, c_ab_v, s_b, c_b);

    b->setTruthValue(SimpleTruthValue::createTV(s_b, c_b));
    return b;
}

std::vector<Handle> PLNRuleLibrary::tryUnify(
    const Handle& rule_atom,
    const std::vector<Handle>& premises) const
{
    (void)rule_atom;
    (void)premises;
    // Placeholder: full unification requires URE
    // For now we return empty and rely on the explicit methods
    return {};
}

// ---------------------------------------------------------------------------
// Inference (forward / backward chaining)
// ---------------------------------------------------------------------------

InferenceResult PLNRuleLibrary::applyRule(const std::string& rule_name,
                                           const std::vector<Handle>& premises)
{
    std::lock_guard<std::mutex> lock(_mutex);
    InferenceResult result;
    auto it = _rules.find(rule_name);
    if (it == _rules.end() || !it->second.active) {
        result.error_message = "Rule not found or inactive: " + rule_name;
        return result;
    }

    // Dispatch to specific implementations
    if (rule_name == "deduction" && premises.size() >= 2) {
        Handle ac = applyDeduction(premises[0], premises[1]);
        if (ac) {
            result.conclusion = ac;
            result.premises = premises;
            result.rule_used = rule_name;
            TruthValuePtr tv = ac->getTruthValue();
            result.strength = tv ? tv->get_mean() : 0.0;
            result.confidence = tv ? tv->get_confidence() : 0.0;
            result.success = true;
            it->second.fire_count++;
        }
    } else if (rule_name == "modus-ponens" && premises.size() >= 2) {
        Handle b = applyModusPonens(premises[0], premises[1]);
        if (b) {
            result.conclusion = b;
            result.premises = premises;
            result.rule_used = rule_name;
            TruthValuePtr tv = b->getTruthValue();
            result.strength = tv ? tv->get_mean() : 0.0;
            result.confidence = tv ? tv->get_confidence() : 0.0;
            result.success = true;
            it->second.fire_count++;
        }
    } else {
        result.error_message = "Rule '" + rule_name + "' could not fire on given premises";
    }
    return result;
}

std::vector<InferenceResult> PLNRuleLibrary::shallowInfer(
    const std::vector<Handle>& premises)
{
    // Take a snapshot of the active rule names under the lock,
    // then release it before calling applyRule (which acquires the lock).
    std::vector<std::string> active_names;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto& [name, rule] : _rules)
            if (rule.active) active_names.push_back(name);
    }

    std::vector<InferenceResult> results;
    for (auto& name : active_names) {
        for (size_t i = 0; i < premises.size(); ++i) {
            for (size_t j = 0; j < premises.size(); ++j) {
                if (i == j) continue;
                auto r = applyRule(name, {premises[i], premises[j]});
                if (r.success) results.push_back(std::move(r));
            }
        }
    }
    return results;
}

std::vector<InferenceResult> PLNRuleLibrary::forwardChain(
    const std::vector<Handle>& premises,
    size_t max_steps)
{
    std::vector<InferenceResult> all_results;
    std::vector<Handle> working(premises);

    for (size_t step = 0; step < max_steps; ++step) {
        auto new_results = shallowInfer(working);
        if (new_results.empty()) break;
        for (auto& r : new_results) {
            if (r.success && r.conclusion) {
                working.push_back(r.conclusion);
                all_results.push_back(std::move(r));
            }
        }
    }
    return all_results;
}

std::vector<InferenceResult> PLNRuleLibrary::backwardChain(
    const Handle& goal,
    size_t max_steps)
{
    // Simplified backward chaining: find ImplicationLinks whose consequent
    // matches goal and recursively try to prove the antecedents.
    std::vector<InferenceResult> all_results;
    if (!goal || max_steps == 0) return all_results;

    // Find all implication links with goal as consequent
    HandleSeq impls;
    _atomspace->get_handles_by_type(impls, IMPLICATION_LINK);
    for (auto& impl : impls) {
        if (impl->getArity() < 2) continue;
        Handle cons = impl->getOutgoingAtom(1);
        if (cons != goal) continue;

        Handle ant = impl->getOutgoingAtom(0);
        // Treat antecedent as a premise and try to prove it recursively
        InferenceResult r;
        r.conclusion = goal;
        r.premises = {ant};
        r.rule_used = "backward-implication";
        TruthValuePtr tv = impl->getTruthValue();
        r.strength = tv ? tv->get_mean() : 0.5;
        r.confidence = tv ? tv->get_confidence() : 0.1;
        r.success = true;
        all_results.push_back(std::move(r));

        // Recurse
        auto sub = backwardChain(ant, max_steps - 1);
        all_results.insert(all_results.end(), sub.begin(), sub.end());
    }
    return all_results;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

std::string PLNRuleLibrary::getStatsSummary() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    size_t active_count = 0;
    size_t total_fires = 0;
    for (auto& [name, rule] : _rules) {
        if (rule.active) ++active_count;
        total_fires += rule.fire_count;
    }
    std::ostringstream ss;
    ss << "[PLNRuleLibrary] rules=" << _rules.size()
       << " active=" << active_count
       << " total_fires=" << total_fires
       << " initialized=" << _initialized;
    return ss.str();
}

bool PLNRuleLibrary::isHealthy() const
{
    return _initialized && (_atomspace != nullptr);
}
