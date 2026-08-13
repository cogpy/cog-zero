/*
 * agentzero-core/src/ReasoningEngine.cpp
 *
 * Forward-chaining reasoning with TruthValue uncertainty.
 * Optionally degrades gracefully when PLN/URE are unavailable.
 */
#include "opencog/agentzero/ReasoningEngine.h"
#include "opencog/agentzero/AgentZeroCore.h"

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <algorithm>
#include <sstream>

using namespace opencog;
using namespace opencog::agentzero;

ReasoningEngine::ReasoningEngine(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(std::move(atomspace))
{
    if (!_atomspace) {
        throw std::invalid_argument("ReasoningEngine requires a valid AtomSpace");
    }
    _reasoning_root = _atomspace->add_node(CONCEPT_NODE, "ReasoningEngine");
    installDefaultRules();
}

ReasoningEngine::~ReasoningEngine() = default;

void ReasoningEngine::installDefaultRules()
{
    // Modus ponens-style: if A and (Implication A B) then B
    ReasoningRule modus;
    modus.name = "modus_ponens";
    modus.rule_type = "pln";
    modus.weight = 1.0;
    modus.applicability_check = [](const std::vector<Handle>& facts) {
        return facts.size() >= 1;
    };
    modus.apply = [this](AtomSpacePtr as, const std::vector<Handle>& facts) -> ReasoningResult {
        ReasoningResult result;
        // Look for implication links in the atomspace involving premises
        auto implications = as->get_handles_by_type(IMPLICATION_LINK);
        for (const auto& impl : implications) {
            if (!impl || !impl->is_link() || impl->get_arity() < 2) continue;
            auto out = impl->getOutgoingSet();
            Handle ante = out[0];
            Handle cons = out[1];
            bool ante_present = false;
            for (const auto& f : facts) {
                if (f == ante) { ante_present = true; break; }
                // Also match by name for concept nodes
                if (f && ante && f->is_node() && ante->is_node() &&
                    f->get_name() == ante->get_name()) {
                    ante_present = true;
                    break;
                }
            }
            if (!ante_present) continue;

            double s_impl = SimpleTruthValue::getStrength(impl);
            double c_impl = SimpleTruthValue::getConfidence(impl);
            double s_ante = SimpleTruthValue::getStrength(ante);
            double c_ante = SimpleTruthValue::getConfidence(ante);
            double strength = s_impl * s_ante;
            double confidence = std::min(c_impl, c_ante) * 0.9;
            if (confidence < _confidence_threshold || strength < _truth_threshold) continue;

            SimpleTruthValue::setTV(cons, strength, confidence);
            result.conclusion = cons;
            result.strength = strength;
            result.confidence = confidence;
            result.explanation = "modus_ponens";
            result.support = {ante, impl};
            return result;
        }
        return result;
    };
    _rules.push_back(std::move(modus));

    // Inheritance transitivity sketch: A->B and B->C => A->C
    ReasoningRule inh;
    inh.name = "inheritance_transitivity";
    inh.rule_type = "pln";
    inh.weight = 0.8;
    inh.applicability_check = [](const std::vector<Handle>&) { return true; };
    inh.apply = [this](AtomSpacePtr as, const std::vector<Handle>&) -> ReasoningResult {
        ReasoningResult result;
        auto links = as->get_handles_by_type(INHERITANCE_LINK);
        for (size_t i = 0; i < links.size(); ++i) {
            if (!links[i] || links[i]->get_arity() < 2) continue;
            auto a = links[i]->getOutgoingSet();
            for (size_t j = 0; j < links.size(); ++j) {
                if (i == j || !links[j] || links[j]->get_arity() < 2) continue;
                auto b = links[j]->getOutgoingSet();
                if (a[1] != b[0]) continue;
                Handle conclusion = as->add_link(INHERITANCE_LINK, HandleSeq{a[0], b[1]});
                double s = SimpleTruthValue::getStrength(links[i]) *
                           SimpleTruthValue::getStrength(links[j]);
                double c = std::min(SimpleTruthValue::getConfidence(links[i]),
                                    SimpleTruthValue::getConfidence(links[j])) * 0.85;
                if (c < _confidence_threshold) continue;
                SimpleTruthValue::setTV(conclusion, s, c);
                result.conclusion = conclusion;
                result.strength = s;
                result.confidence = c;
                result.explanation = "inheritance_transitivity";
                result.support = {links[i], links[j]};
                return result;
            }
        }
        return result;
    };
    _rules.push_back(std::move(inh));
}

std::vector<ReasoningEngine::ReasoningResult>
ReasoningEngine::reason(const std::vector<Handle>& premises, ReasoningMode mode)
{
    std::lock_guard<std::mutex> lock(_mu);
    switch (mode) {
        case ReasoningMode::BACKWARD_CHAINING:
            return backwardChain(premises);
        case ReasoningMode::FORWARD_CHAINING:
        case ReasoningMode::MIXED:
        default:
            return forwardChain(premises);
    }
}

std::vector<ReasoningEngine::ReasoningResult>
ReasoningEngine::forwardChain(const std::vector<Handle>& premises)
{
    std::vector<ReasoningResult> results;
    std::vector<Handle> facts = premises;
    int iterations = 0;
    while (iterations++ < _max_iterations) {
        bool progressed = false;
        for (const auto& rule : _rules) {
            if (rule.applicability_check && !rule.applicability_check(facts)) continue;
            if (!rule.apply) continue;
            auto r = rule.apply(_atomspace, facts);
            if (!r.conclusion) continue;
            // Avoid duplicates
            bool known = false;
            for (const auto& f : facts) if (f == r.conclusion) { known = true; break; }
            if (known) continue;
            recordConclusion(r);
            results.push_back(r);
            facts.push_back(r.conclusion);
            progressed = true;
        }
        if (!progressed) break;
    }
    return results;
}

std::vector<ReasoningEngine::ReasoningResult>
ReasoningEngine::backwardChain(const std::vector<Handle>& goals)
{
    // Simplified: treat goals as premises for a reverse search over implications
    std::vector<ReasoningResult> results;
    auto implications = _atomspace->get_handles_by_type(IMPLICATION_LINK);
    for (const auto& goal : goals) {
        for (const auto& impl : implications) {
            if (!impl || impl->get_arity() < 2) continue;
            auto out = impl->getOutgoingSet();
            if (out[1] != goal) continue;
            ReasoningResult r;
            r.conclusion = out[0]; // required premise
            r.explanation = "backward_support";
            r.strength = SimpleTruthValue::getStrength(impl);
            r.confidence = SimpleTruthValue::getConfidence(impl);
            r.support = {impl, goal};
            results.push_back(r);
        }
    }
    return results;
}

std::vector<ReasoningEngine::Hypothesis>
ReasoningEngine::generateHypotheses(const std::vector<Handle>& observations)
{
    std::vector<Hypothesis> hyps;
    for (const auto& obs : observations) {
        if (!obs) continue;
        Hypothesis h;
        h.atom = _atomspace->add_node(CONCEPT_NODE,
            "Hypothesis_for_" + (obs->is_node() ? obs->get_name() : "link"));
        h.explanation = "Abductive hypothesis for observation";
        h.plausibility = 0.5;
        SimpleTruthValue::setTV(h.atom, h.plausibility, 0.5);
        hyps.push_back(h);
    }
    return hyps;
}

bool ReasoningEngine::addReasoningRule(const ReasoningRule& rule)
{
    std::lock_guard<std::mutex> lock(_mu);
    _rules.push_back(rule);
    return true;
}

size_t ReasoningEngine::ruleCount() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _rules.size();
}

void ReasoningEngine::configurePLN(bool enable, double confidence_threshold,
                                   double truth_threshold)
{
    _pln_enabled = enable;
    _confidence_threshold = confidence_threshold;
    _truth_threshold = truth_threshold;
}

void ReasoningEngine::configureURE(bool enable, int max_iterations,
                                   double complexity_penalty)
{
    _ure_enabled = enable;
    _max_iterations = max_iterations;
    _complexity_penalty = complexity_penalty;
}

bool ReasoningEngine::processReasoningCycle()
{
    if (!_atomspace) return false;
    // Use all concept nodes as weak premises for a cycle
    auto premises = _atomspace->get_handles_by_type(CONCEPT_NODE);
    if (premises.size() > 64) premises.resize(64);
    auto results = reason(premises, ReasoningMode::FORWARD_CHAINING);
    return true;
}

Handle ReasoningEngine::recordConclusion(const ReasoningResult& result)
{
    if (!result.conclusion) return Handle::UNDEFINED;
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "concluded");
    Handle eval = _atomspace->add_link(
        EVALUATION_LINK,
        HandleSeq{pred, _atomspace->add_link(LIST_LINK, HandleSeq{result.conclusion, _reasoning_root})});
    SimpleTruthValue::setTV(eval, result.strength, result.confidence);
    _last_conclusion = result.conclusion;
    ++_conclusion_count;
    return eval;
}

std::string ReasoningEngine::getStatusInfo() const
{
    std::ostringstream oss;
    oss << "{\"rules\":" << ruleCount()
        << ",\"conclusions\":" << _conclusion_count
        << ",\"pln\":" << (_pln_enabled ? "true" : "false")
        << ",\"ure\":" << (_ure_enabled ? "true" : "false") << "}";
    return oss.str();
}
