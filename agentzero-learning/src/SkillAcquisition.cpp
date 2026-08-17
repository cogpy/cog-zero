/*
 * src/SkillAcquisition.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * SkillAcquisition — learn reusable skills from experience
 * Part of Phase 5: Continuous Learning & Adaptation
 * Part of the AGENT-ZERO-GENESIS project
 */

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/SkillAcquisition.h"

using namespace opencog;
using namespace opencog::agentzero;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SkillAcquisition::SkillAcquisition(AtomSpacePtr atomspace,
                                   const SkillAcquisitionConfig& config)
    : _atomspace(atomspace)
    , _config(config)
    , _skill_base(Handle::UNDEFINED)
{
    if (!_atomspace) {
        throw std::runtime_error("[SkillAcquisition] AtomSpace pointer must not be null");
    }
    logger().info() << "[SkillAcquisition] Initialising skill acquisition framework";
    initializeSkillBase();
}

SkillAcquisition::~SkillAcquisition()
{
    logger().info() << "[SkillAcquisition] Shutting down with "
                    << _skills.size() << " skills in library";
}

// ---------------------------------------------------------------------------
// Internal initialisation
// ---------------------------------------------------------------------------

void SkillAcquisition::initializeSkillBase()
{
    _skill_base = _atomspace->add_node(CONCEPT_NODE, "SkillLibrary");
    logger().info() << "[SkillAcquisition] Skill library initialised in AtomSpace";
}

Handle SkillAcquisition::createSkillAtom(const Skill& skill)
{
    // Confidence grows with application count (capped at 0.99)
    float confidence = static_cast<float>(
        std::min(0.99, 0.5 + 0.4 * (1.0 - std::exp(-0.1 * skill.application_count))));
    Handle skill_atom = _atomspace->add_node(CONCEPT_NODE, "Skill_" + skill.name);
    skill_atom->setTruthValue(
        SimpleTruthValue::createTV(static_cast<float>(skill.success_rate), confidence));
    _atomspace->add_link(MEMBER_LINK, skill_atom, _skill_base);
    return skill_atom;
}

// ---------------------------------------------------------------------------
// Skill extraction
// ---------------------------------------------------------------------------

Skill SkillAcquisition::extractSkillFromExperiences(const std::vector<Experience>& related)
{
    Skill skill;
    skill.name = "LearnedSkill_" + std::to_string(_skills.size());
    skill.description = "Skill extracted from " + std::to_string(related.size())
                        + " experience(s)";
    skill.proficiency  = SkillProficiency::NOVICE;
    skill.success_rate = 0.0;
    skill.application_count = 0;
    skill.transfer_score = 0.5;

    // Collect common context and task atoms from successful experiences
    std::map<Handle, int> context_votes, task_votes;
    for (const auto& exp : related) {
        if (exp.context != Handle::UNDEFINED) context_votes[exp.context]++;
        if (exp.task    != Handle::UNDEFINED) task_votes[exp.task]++;
    }

    // Most common context becomes a precondition
    for (const auto& [h, cnt] : context_votes) {
        if (cnt >= _config.min_demonstrations) {
            skill.preconditions.push_back(h);
        }
    }

    // Most common task becomes part of the action sequence
    for (const auto& [h, cnt] : task_votes) {
        if (cnt >= _config.min_demonstrations) {
            skill.action_sequence.push_back(h);
        }
    }

    // Outcomes of successful experiences become effects
    for (const auto& exp : related) {
        if (exp.type == ExperienceType::SUCCESS && exp.outcome != Handle::UNDEFINED) {
            skill.effects.push_back(exp.outcome);
        }
    }

    // Initial success rate based on fraction of SUCCESS experiences
    int success_count = 0;
    for (const auto& exp : related) {
        if (exp.type == ExperienceType::SUCCESS) ++success_count;
    }
    skill.success_rate = related.empty()
                         ? 0.0
                         : static_cast<double>(success_count) / static_cast<double>(related.size());

    return skill;
}

std::vector<Handle> SkillAcquisition::learnFromExperiences(
    const std::vector<Experience>& experiences)
{
    logger().info() << "[SkillAcquisition] Learning from "
                    << experiences.size() << " experience(s)";

    std::vector<Handle> new_skill_handles;

    // Group experiences by task atom (each task can produce one skill)
    std::map<Handle, std::vector<Experience>> by_task;
    for (const auto& exp : experiences) {
        by_task[exp.task].push_back(exp);
    }

    for (auto& [task_handle, task_exps] : by_task) {
        if (static_cast<int>(task_exps.size()) < _config.min_demonstrations) continue;

        Skill candidate = extractSkillFromExperiences(task_exps);
        if (candidate.success_rate < _config.min_success_rate) continue;

        // Check whether a similar skill already exists
        bool merged = false;
        for (auto& [h, existing] : _skills) {
            if (isSimilarSkill(existing, candidate)) {
                mergeSkills(existing, candidate);
                merged = true;
                break;
            }
        }

        if (!merged) {
            candidate.id = createSkillAtom(candidate);
            _skills[candidate.id] = candidate;
            new_skill_handles.push_back(candidate.id);
            logger().info() << "[SkillAcquisition] New skill acquired: "
                            << candidate.name
                            << " (success_rate=" << candidate.success_rate << ")";
        }
    }

    logger().info() << "[SkillAcquisition] Learned "
                    << new_skill_handles.size() << " new skill(s)";
    return new_skill_handles;
}

// ---------------------------------------------------------------------------
// Skill refinement
// ---------------------------------------------------------------------------

void SkillAcquisition::refineSkill(const Handle& skill_handle, bool success,
                                    const Handle& context)
{
    auto it = _skills.find(skill_handle);
    if (it == _skills.end()) {
        logger().warn() << "[SkillAcquisition] refineSkill: unknown skill handle";
        return;
    }

    Skill& skill = it->second;
    ++skill.application_count;

    // Exponential moving average of success rate
    double alpha = 0.1;
    skill.success_rate = (1.0 - alpha) * skill.success_rate + alpha * (success ? 1.0 : 0.0);

    // Store context variant if transfer is enabled
    if (_config.enable_skill_transfer && context != Handle::UNDEFINED) {
        std::string key = "ctx_" + std::to_string(skill.context_variants.size());
        skill.context_variants[key] = context;
    }

    updateProficiency(skill);
}

void SkillAcquisition::updateProficiency(Skill& skill)
{
    if      (skill.success_rate >= 0.95 && skill.application_count >= 20)
        skill.proficiency = SkillProficiency::EXPERT;
    else if (skill.success_rate >= 0.80 && skill.application_count >= 10)
        skill.proficiency = SkillProficiency::ADVANCED;
    else if (skill.success_rate >= 0.60 && skill.application_count >= 5)
        skill.proficiency = SkillProficiency::INTERMEDIATE;
    else if (skill.success_rate >= 0.40)
        skill.proficiency = SkillProficiency::BEGINNER;
    else
        skill.proficiency = SkillProficiency::NOVICE;
}

// ---------------------------------------------------------------------------
// Skill retrieval
// ---------------------------------------------------------------------------

std::vector<Skill> SkillAcquisition::getApplicableSkills(const Handle& context,
                                                          int max_results) const
{
    std::vector<Skill> applicable;
    for (const auto& [h, skill] : _skills) {
        if (contextMatches(skill, context)) {
            applicable.push_back(skill);
        }
    }

    // Sort by proficiency (descending) then success rate
    std::sort(applicable.begin(), applicable.end(), [](const Skill& a, const Skill& b) {
        if (a.proficiency != b.proficiency)
            return static_cast<int>(a.proficiency) > static_cast<int>(b.proficiency);
        return a.success_rate > b.success_rate;
    });

    if (max_results > 0 && static_cast<int>(applicable.size()) > max_results) {
        applicable.resize(static_cast<size_t>(max_results));
    }
    return applicable;
}

Skill SkillAcquisition::getSkill(const Handle& skill_handle) const
{
    auto it = _skills.find(skill_handle);
    return (it != _skills.end()) ? it->second : Skill{};
}

std::vector<Skill> SkillAcquisition::getAllSkills() const
{
    std::vector<Skill> all;
    all.reserve(_skills.size());
    for (const auto& [h, s] : _skills) all.push_back(s);
    return all;
}

// ---------------------------------------------------------------------------
// Transfer learning
// ---------------------------------------------------------------------------

Handle SkillAcquisition::transferSkill(const Handle& skill_handle,
                                        const Handle& new_context)
{
    auto it = _skills.find(skill_handle);
    if (it == _skills.end()) return Handle::UNDEFINED;

    const Skill& original = it->second;

    // Create a specialised copy for the new context
    Skill transferred = original;
    transferred.name = original.name + "_ctx" + std::to_string(_skills.size());
    transferred.description = "Transfer of " + original.name + " to new context";
    transferred.preconditions.push_back(new_context);
    transferred.proficiency    = SkillProficiency::NOVICE; // reset proficiency
    transferred.success_rate   = original.success_rate * 0.7; // discount on transfer
    transferred.transfer_score = original.transfer_score;
    transferred.application_count = 0;

    transferred.id = createSkillAtom(transferred);
    _skills[transferred.id] = transferred;

    logger().info() << "[SkillAcquisition] Transferred skill '" << original.name
                    << "' to new context as '" << transferred.name << "'";
    return transferred.id;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

std::map<std::string, double> SkillAcquisition::getSkillStatistics() const
{
    std::map<std::string, double> stats;
    stats["total_skills"] = static_cast<double>(_skills.size());

    if (_skills.empty()) {
        stats["average_success_rate"]    = 0.0;
        stats["average_applications"]    = 0.0;
        stats["expert_skill_count"]      = 0.0;
        stats["advanced_skill_count"]    = 0.0;
        stats["intermediate_skill_count"]= 0.0;
        stats["beginner_skill_count"]    = 0.0;
        stats["novice_skill_count"]      = 0.0;
        return stats;
    }

    double total_success = 0.0, total_apps = 0.0;
    std::map<SkillProficiency, int> prof_counts;
    for (const auto& [h, s] : _skills) {
        total_success += s.success_rate;
        total_apps    += s.application_count;
        prof_counts[s.proficiency]++;
    }

    double n = static_cast<double>(_skills.size());
    stats["average_success_rate"]     = total_success / n;
    stats["average_applications"]     = total_apps    / n;
    stats["expert_skill_count"]       = static_cast<double>(prof_counts[SkillProficiency::EXPERT]);
    stats["advanced_skill_count"]     = static_cast<double>(prof_counts[SkillProficiency::ADVANCED]);
    stats["intermediate_skill_count"] = static_cast<double>(prof_counts[SkillProficiency::INTERMEDIATE]);
    stats["beginner_skill_count"]     = static_cast<double>(prof_counts[SkillProficiency::BEGINNER]);
    stats["novice_skill_count"]       = static_cast<double>(prof_counts[SkillProficiency::NOVICE]);
    return stats;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool SkillAcquisition::isSimilarSkill(const Skill& a, const Skill& b) const
{
    // Overlap in preconditions and action sequences
    int shared = 0, total = 0;
    for (const auto& h : a.preconditions) {
        ++total;
        if (std::find(b.preconditions.begin(), b.preconditions.end(), h)
                != b.preconditions.end()) ++shared;
    }
    for (const auto& h : b.preconditions) {
        ++total;
        if (std::find(a.preconditions.begin(), a.preconditions.end(), h)
                != a.preconditions.end()) ++shared;
    }
    if (total == 0) return false;
    double overlap = static_cast<double>(shared) / static_cast<double>(total);
    return overlap >= _config.similarity_threshold;
}

void SkillAcquisition::mergeSkills(Skill& existing, const Skill& candidate)
{
    // Blend success rate
    double alpha = 0.3;
    existing.success_rate = (1.0 - alpha) * existing.success_rate
                            + alpha * candidate.success_rate;

    // Add any new effects
    for (const auto& h : candidate.effects) {
        if (std::find(existing.effects.begin(), existing.effects.end(), h)
                == existing.effects.end()) {
            existing.effects.push_back(h);
        }
    }
    updateProficiency(existing);
    logger().debug() << "[SkillAcquisition] Merged candidate into existing skill '"
                     << existing.name << "'";
}

bool SkillAcquisition::contextMatches(const Skill& skill, const Handle& context) const
{
    if (skill.preconditions.empty()) return true; // No preconditions — always applicable
    if (context == Handle::UNDEFINED) return true;

    for (const auto& pre : skill.preconditions) {
        if (pre == context) return true;
    }
    // Also check context variants
    for (const auto& [key, h] : skill.context_variants) {
        if (h == context) return true;
    }
    return false;
}

std::string SkillAcquisition::proficiencyToString(SkillProficiency p)
{
    switch (p) {
        case SkillProficiency::NOVICE:        return "NOVICE";
        case SkillProficiency::BEGINNER:      return "BEGINNER";
        case SkillProficiency::INTERMEDIATE:  return "INTERMEDIATE";
        case SkillProficiency::ADVANCED:      return "ADVANCED";
        case SkillProficiency::EXPERT:        return "EXPERT";
        default:                              return "UNKNOWN";
    }
}
