/*
 * src/ExperienceManager.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ExperienceManager — episodic memory for agent trajectories
 * Part of Phase 5: Continuous Learning & Adaptation
 * Part of the AGENT-ZERO-GENESIS project
 */

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/ExperienceManager.h"

using namespace opencog;
using namespace opencog::agentzero;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

ExperienceManager::ExperienceManager(AtomSpacePtr atomspace,
                                     size_t max_experiences,
                                     double importance_threshold,
                                     std::chrono::hours retention_period)
    : _atomspace(atomspace)
    , _max_experiences(max_experiences)
    , _importance_threshold(importance_threshold)
    , _retention_period(retention_period)
    , _experience_context(Handle::UNDEFINED)
    , _memory_link(Handle::UNDEFINED)
    , _initialized(false)
{
    if (!_atomspace) {
        throw std::runtime_error("[ExperienceManager] AtomSpace pointer must not be null");
    }
    logger().info() << "[ExperienceManager] Initialising with max_experiences=" << max_experiences
                    << ", importance_threshold=" << importance_threshold;
    initialize();
}

ExperienceManager::~ExperienceManager()
{
    logger().info() << "[ExperienceManager] Shutting down with "
                    << _experiences.size() << " experiences stored";
}

// ---------------------------------------------------------------------------
// Internal initialisation
// ---------------------------------------------------------------------------

void ExperienceManager::initialize()
{
    _experience_context = _atomspace->add_node(CONCEPT_NODE, "ExperienceContext");
    _memory_link = _atomspace->add_link(
        EVALUATION_LINK,
        _atomspace->add_node(PREDICATE_NODE, "ExperienceMemory"),
        _experience_context);
    _initialized = true;
    logger().info() << "[ExperienceManager] AtomSpace structures initialised";
}

Handle ExperienceManager::createExperienceAtom(const Experience& exp)
{
    // Each experience is anchored by a unique ConceptNode
    std::string label = "Experience_" + experienceTypeToString(exp.type) + "_"
                        + std::to_string(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                exp.timestamp.time_since_epoch()).count());

    Handle exp_atom = _atomspace->add_node(CONCEPT_NODE, label);
    exp_atom->setTruthValue(SimpleTruthValue::createTV(
        static_cast<float>(exp.importance), 0.9f));

    // Connect to the experience context
    _atomspace->add_link(MEMBER_LINK, exp_atom, _experience_context);

    return exp_atom;
}

void ExperienceManager::indexExperience(const Experience& exp, size_t index)
{
    if (exp.id != Handle::UNDEFINED) {
        _experience_index[exp.id] = index;
    }
    _type_index[exp.type].push_back(index);
    if (exp.context != Handle::UNDEFINED) {
        _context_index[exp.context].push_back(index);
    }
}

void ExperienceManager::removeExperienceFromIndices(size_t index)
{
    if (index >= _experiences.size()) return;
    const Experience& exp = _experiences[index];

    if (exp.id != Handle::UNDEFINED) {
        _experience_index.erase(exp.id);
    }

    auto removeFromVec = [&](std::vector<size_t>& vec) {
        vec.erase(std::remove(vec.begin(), vec.end(), index), vec.end());
    };

    auto& tv = _type_index[exp.type];
    removeFromVec(tv);

    if (exp.context != Handle::UNDEFINED) {
        auto cit = _context_index.find(exp.context);
        if (cit != _context_index.end()) {
            removeFromVec(cit->second);
        }
    }
}

bool ExperienceManager::shouldRetainExperience(const Experience& exp) const
{
    if (exp.importance < _importance_threshold) return false;

    auto age = std::chrono::system_clock::now() - exp.timestamp;
    if (std::chrono::duration_cast<std::chrono::hours>(age) > _retention_period) {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Recording
// ---------------------------------------------------------------------------

Handle ExperienceManager::recordExperience(ExperienceType type,
                                           const Handle& context,
                                           const Handle& task,
                                           const Handle& outcome,
                                           double importance)
{
    Experience exp;
    exp.type      = type;
    exp.context   = context;
    exp.task      = task;
    exp.outcome   = outcome;
    exp.timestamp = std::chrono::system_clock::now();
    exp.importance = std::max(0.0, std::min(1.0, importance));

    exp.id = createExperienceAtom(exp);

    size_t index = _experiences.size();
    _experiences.push_back(exp);
    indexExperience(exp, index);

    logger().debug() << "[ExperienceManager] Recorded "
                     << experienceTypeToString(type)
                     << " experience (importance=" << exp.importance << ")";

    if (_experiences.size() > _max_experiences) {
        consolidateMemory();
    }

    return exp.id;
}

// ---------------------------------------------------------------------------
// Retrieval
// ---------------------------------------------------------------------------

std::vector<Experience> ExperienceManager::queryExperiences(const ExperienceQuery& query) const
{
    std::vector<size_t> candidates;

    // Start with type-filtered set
    auto tit = _type_index.find(query.type_filter);
    if (tit != _type_index.end()) {
        candidates = tit->second;
    } else {
        candidates.resize(_experiences.size());
        for (size_t i = 0; i < _experiences.size(); ++i) candidates[i] = i;
    }

    std::vector<Experience> results;
    for (size_t idx : candidates) {
        if (idx >= _experiences.size()) continue;
        const Experience& exp = _experiences[idx];

        if (exp.importance < query.min_importance) continue;

        if (query.context_filter != Handle::UNDEFINED
            && exp.context != query.context_filter) continue;

        if (query.task_filter != Handle::UNDEFINED
            && exp.task != query.task_filter) continue;

        // Time range — only apply if non-default
        static const std::chrono::system_clock::time_point epoch{};
        if (query.start_time != epoch && exp.timestamp < query.start_time) continue;
        if (query.end_time   != epoch && exp.timestamp > query.end_time)   continue;

        results.push_back(exp);
        if (query.max_results > 0 && static_cast<int>(results.size()) >= query.max_results) {
            break;
        }
    }
    return results;
}

std::vector<Experience> ExperienceManager::getExperiencesByType(ExperienceType type,
                                                                int max_count) const
{
    auto it = _type_index.find(type);
    if (it == _type_index.end()) return {};

    std::vector<Experience> results;
    for (size_t idx : it->second) {
        if (idx < _experiences.size()) {
            results.push_back(_experiences[idx]);
        }
        if (max_count > 0 && static_cast<int>(results.size()) >= max_count) break;
    }
    return results;
}

std::vector<Experience> ExperienceManager::getRecentExperiences(
    std::chrono::hours time_window, int max_count) const
{
    auto cutoff = std::chrono::system_clock::now() - time_window;
    std::vector<Experience> results;

    // Iterate in reverse order (most recent first)
    for (int i = static_cast<int>(_experiences.size()) - 1; i >= 0; --i) {
        const auto& exp = _experiences[static_cast<size_t>(i)];
        if (exp.timestamp >= cutoff) {
            results.push_back(exp);
        }
        if (max_count > 0 && static_cast<int>(results.size()) >= max_count) break;
    }
    return results;
}

std::vector<Experience> ExperienceManager::getExperiencesByContext(const Handle& context,
                                                                    int max_count) const
{
    auto cit = _context_index.find(context);
    if (cit == _context_index.end()) return {};

    std::vector<Experience> results;
    for (size_t idx : cit->second) {
        if (idx < _experiences.size()) {
            results.push_back(_experiences[idx]);
        }
        if (max_count > 0 && static_cast<int>(results.size()) >= max_count) break;
    }
    return results;
}

std::vector<Experience> ExperienceManager::findSimilarExperiences(const Experience& target,
                                                                   int max_results) const
{
    std::vector<std::pair<double, size_t>> scored;
    scored.reserve(_experiences.size());

    for (size_t i = 0; i < _experiences.size(); ++i) {
        double sim = calculateSimilarity(target, _experiences[i]);
        if (sim > 0.0) scored.emplace_back(sim, i);
    }

    // Sort descending by similarity
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<Experience> results;
    for (const auto& [sim, idx] : scored) {
        results.push_back(_experiences[idx]);
        if (max_results > 0 && static_cast<int>(results.size()) >= max_results) break;
    }
    return results;
}

// ---------------------------------------------------------------------------
// Importance and memory management
// ---------------------------------------------------------------------------

bool ExperienceManager::updateExperienceImportance(const Handle& experience_handle,
                                                    double new_importance)
{
    auto it = _experience_index.find(experience_handle);
    if (it == _experience_index.end()) return false;

    _experiences[it->second].importance = std::max(0.0, std::min(1.0, new_importance));
    return true;
}

void ExperienceManager::consolidateMemory()
{
    logger().info() << "[ExperienceManager] Consolidating memory ("
                    << _experiences.size() << " experiences)";

    // Mark experiences to remove (expired or below importance threshold)
    std::vector<size_t> to_remove;
    for (size_t i = 0; i < _experiences.size(); ++i) {
        if (!shouldRetainExperience(_experiences[i])) {
            to_remove.push_back(i);
        }
    }

    // If still over limit, remove lowest-importance ones
    if (_experiences.size() - to_remove.size() > _max_experiences) {
        // Collect candidates not already marked
        std::vector<std::pair<double, size_t>> sortable;
        std::set<size_t> already(to_remove.begin(), to_remove.end());
        for (size_t i = 0; i < _experiences.size(); ++i) {
            if (already.find(i) == already.end()) {
                sortable.emplace_back(_experiences[i].importance, i);
            }
        }
        std::sort(sortable.begin(), sortable.end()); // ascending importance
        size_t extra = (_experiences.size() - to_remove.size()) - _max_experiences;
        for (size_t k = 0; k < extra && k < sortable.size(); ++k) {
            to_remove.push_back(sortable[k].second);
        }
    }

    // Sort indices descending so erasure does not invalidate remaining indices.
    // Indices are fully rebuilt after the bulk erase (cheaper than per-element
    // index surgery when many entries are removed).
    std::sort(to_remove.rbegin(), to_remove.rend());
    for (size_t idx : to_remove) {
        _experiences.erase(_experiences.begin() + static_cast<long>(idx));
    }

    _experience_index.clear();
    _type_index.clear();
    _context_index.clear();
    for (size_t i = 0; i < _experiences.size(); ++i) {
        indexExperience(_experiences[i], i);
    }

    logger().info() << "[ExperienceManager] Consolidation complete ("
                    << _experiences.size() << " experiences remain)";
}

void ExperienceManager::clearAllExperiences()
{
    _experiences.clear();
    _experience_index.clear();
    _type_index.clear();
    _context_index.clear();
    logger().info() << "[ExperienceManager] All experiences cleared";
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

std::map<std::string, double> ExperienceManager::getExperienceStatistics() const
{
    std::map<std::string, double> stats;
    stats["total_experiences"] = static_cast<double>(_experiences.size());

    double total_importance = 0.0;
    std::map<ExperienceType, int> type_counts;
    for (const auto& exp : _experiences) {
        total_importance += exp.importance;
        type_counts[exp.type]++;
    }
    stats["average_importance"] = _experiences.empty()
                                   ? 0.0
                                   : total_importance / static_cast<double>(_experiences.size());

    stats["action_outcome_count"]  = static_cast<double>(type_counts[ExperienceType::ACTION_OUTCOME]);
    stats["observation_count"]     = static_cast<double>(type_counts[ExperienceType::OBSERVATION]);
    stats["interaction_count"]     = static_cast<double>(type_counts[ExperienceType::INTERACTION]);
    stats["problem_solving_count"] = static_cast<double>(type_counts[ExperienceType::PROBLEM_SOLVING]);
    stats["success_count"]         = static_cast<double>(type_counts[ExperienceType::SUCCESS]);
    stats["failure_count"]         = static_cast<double>(type_counts[ExperienceType::FAILURE]);
    stats["discovery_count"]       = static_cast<double>(type_counts[ExperienceType::DISCOVERY]);

    return stats;
}

// ---------------------------------------------------------------------------
// Configuration setters
// ---------------------------------------------------------------------------

void ExperienceManager::setMaxExperiences(size_t max_experiences)
{
    _max_experiences = max_experiences;
}

void ExperienceManager::setImportanceThreshold(double threshold)
{
    _importance_threshold = std::max(0.0, std::min(1.0, threshold));
}

void ExperienceManager::setRetentionPeriod(std::chrono::hours period)
{
    _retention_period = period;
}

// ---------------------------------------------------------------------------
// Utility / statics
// ---------------------------------------------------------------------------

std::string ExperienceManager::experienceTypeToString(ExperienceType type)
{
    switch (type) {
        case ExperienceType::ACTION_OUTCOME:  return "ACTION_OUTCOME";
        case ExperienceType::OBSERVATION:     return "OBSERVATION";
        case ExperienceType::INTERACTION:     return "INTERACTION";
        case ExperienceType::PROBLEM_SOLVING: return "PROBLEM_SOLVING";
        case ExperienceType::SUCCESS:         return "SUCCESS";
        case ExperienceType::FAILURE:         return "FAILURE";
        case ExperienceType::DISCOVERY:       return "DISCOVERY";
        default:                              return "UNKNOWN";
    }
}

ExperienceType ExperienceManager::stringToExperienceType(const std::string& type_str)
{
    if (type_str == "ACTION_OUTCOME")  return ExperienceType::ACTION_OUTCOME;
    if (type_str == "OBSERVATION")     return ExperienceType::OBSERVATION;
    if (type_str == "INTERACTION")     return ExperienceType::INTERACTION;
    if (type_str == "PROBLEM_SOLVING") return ExperienceType::PROBLEM_SOLVING;
    if (type_str == "SUCCESS")         return ExperienceType::SUCCESS;
    if (type_str == "FAILURE")         return ExperienceType::FAILURE;
    if (type_str == "DISCOVERY")       return ExperienceType::DISCOVERY;
    return ExperienceType::OBSERVATION; // default
}

double ExperienceManager::calculateSimilarity(const Experience& a, const Experience& b)
{
    double score = 0.0;

    // Same type contributes a base score
    if (a.type == b.type) score += 0.3;

    // Shared context
    if (a.context != Handle::UNDEFINED && a.context == b.context) score += 0.4;

    // Shared task
    if (a.task != Handle::UNDEFINED && a.task == b.task) score += 0.3;

    return score;
}
