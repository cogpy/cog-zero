/*
 * agentzero-core/src/KnowledgeIntegrator.cpp
 */
#include "opencog/agentzero/KnowledgeIntegrator.h"
#include "opencog/agentzero/AgentZeroCore.h"

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <algorithm>
#include <sstream>

using namespace opencog;
using namespace opencog::agentzero;

KnowledgeIntegrator::KnowledgeIntegrator(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(std::move(atomspace))
{
    if (!_atomspace) {
        throw std::invalid_argument("KnowledgeIntegrator requires a valid AtomSpace");
    }
    _knowledge_base = _atomspace->add_node(CONCEPT_NODE, "KnowledgeBase");
    _semantic_network = _atomspace->add_node(CONCEPT_NODE, "SemanticNetwork");
}

KnowledgeIntegrator::~KnowledgeIntegrator() = default;

Handle KnowledgeIntegrator::ensureConcept(const std::string& name)
{
    return _atomspace->add_node(CONCEPT_NODE, name);
}

Handle KnowledgeIntegrator::encodeItem(const KnowledgeItem& item)
{
    Handle key = ensureConcept("Knowledge:" + item.key);
    Handle content = ensureConcept("Content:" + item.content);
    Handle type_node = ensureConcept("KnowledgeType:" + std::to_string(static_cast<int>(item.type)));
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "knows");
    Handle list = _atomspace->add_link(LIST_LINK, HandleSeq{key, content, type_node, _knowledge_base});
    Handle eval = _atomspace->add_link(EVALUATION_LINK, HandleSeq{pred, list});
    SimpleTruthValue::setTV(eval, item.strength, item.confidence);

    // Semantic network membership
    Handle member = _atomspace->add_link(MEMBER_LINK, HandleSeq{key, _semantic_network});
    SimpleTruthValue::setTV(member, item.strength, item.confidence);
    return eval;
}

Handle KnowledgeIntegrator::addKnowledge(const std::string& key, const std::string& content,
                                         KnowledgeType type, double strength, double confidence)
{
    std::lock_guard<std::mutex> lock(_mu);
    KnowledgeItem item;
    item.key = key;
    item.content = content;
    item.type = type;
    item.strength = strength;
    item.confidence = confidence;
    item.atom = encodeItem(item);
    _registry[key] = item;
    _active.insert(item.atom);
    return item.atom;
}

Handle KnowledgeIntegrator::addFact(const std::string& subject, const std::string& predicate,
                                    const std::string& object, double strength, double confidence)
{
    Handle s = ensureConcept(subject);
    Handle p = _atomspace->add_node(PREDICATE_NODE, predicate);
    Handle o = ensureConcept(object);
    Handle list = _atomspace->add_link(LIST_LINK, HandleSeq{s, o});
    Handle eval = _atomspace->add_link(EVALUATION_LINK, HandleSeq{p, list});
    SimpleTruthValue::setTV(eval, strength, confidence);

    std::string key = subject + ":" + predicate + ":" + object;
    std::lock_guard<std::mutex> lock(_mu);
    KnowledgeItem item;
    item.key = key;
    item.content = predicate;
    item.type = KnowledgeType::FACTUAL;
    item.strength = strength;
    item.confidence = confidence;
    item.atom = eval;
    _registry[key] = item;
    _active.insert(eval);
    return eval;
}

std::vector<Handle> KnowledgeIntegrator::semanticSearch(const std::string& query,
                                                        size_t limit) const
{
    std::lock_guard<std::mutex> lock(_mu);
    std::vector<Handle> results;
    for (const auto& kv : _registry) {
        if (kv.first.find(query) != std::string::npos ||
            kv.second.content.find(query) != std::string::npos) {
            results.push_back(kv.second.atom);
            if (results.size() >= limit) break;
        }
    }
    // Also scan AtomSpace concept nodes by name substring
    if (results.size() < limit && _atomspace) {
        auto concepts = _atomspace->get_handles_by_type(CONCEPT_NODE);
        for (const auto& h : concepts) {
            if (!h) continue;
            if (h->get_name().find(query) != std::string::npos) {
                if (std::find(results.begin(), results.end(), h) == results.end()) {
                    results.push_back(h);
                    if (results.size() >= limit) break;
                }
            }
        }
    }
    return results;
}

std::vector<Handle> KnowledgeIntegrator::findByType(KnowledgeType type) const
{
    std::lock_guard<std::mutex> lock(_mu);
    std::vector<Handle> out;
    for (const auto& kv : _registry) {
        if (kv.second.type == type) out.push_back(kv.second.atom);
    }
    return out;
}

std::vector<Handle> KnowledgeIntegrator::minePatterns(size_t min_support) const
{
    // Lightweight pattern mining: predicates that appear at least min_support times
    std::lock_guard<std::mutex> lock(_mu);
    std::map<std::string, size_t> counts;
    std::map<std::string, Handle> samples;
    for (const auto& kv : _registry) {
        counts[kv.second.content]++;
        samples[kv.second.content] = kv.second.atom;
    }
    std::vector<Handle> patterns;
    for (const auto& kv : counts) {
        if (kv.second >= min_support) {
            patterns.push_back(samples[kv.first]);
        }
    }
    return patterns;
}

bool KnowledgeIntegrator::removeKnowledge(const std::string& key)
{
    std::lock_guard<std::mutex> lock(_mu);
    auto it = _registry.find(key);
    if (it == _registry.end()) return false;
    _active.erase(it->second.atom);
    _registry.erase(it);
    return true;
}

size_t KnowledgeIntegrator::knowledgeCount() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _registry.size();
}

bool KnowledgeIntegrator::processKnowledgeIntegration()
{
    // Ensure knowledge base anchors exist and are linked
    if (!_atomspace) return false;
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "knowledge_active");
    Handle eval = _atomspace->add_link(
        EVALUATION_LINK,
        HandleSeq{pred, _atomspace->add_link(LIST_LINK, HandleSeq{_knowledge_base})});
    SimpleTruthValue::setTV(eval, 1.0, 1.0);
    return true;
}

std::string KnowledgeIntegrator::getStatusInfo() const
{
    std::lock_guard<std::mutex> lock(_mu);
    std::ostringstream oss;
    oss << "{\"knowledge_items\":" << _registry.size()
        << ",\"active\":" << _active.size() << "}";
    return oss.str();
}
