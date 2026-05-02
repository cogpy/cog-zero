/*
 * opencog/agentzero/SkillAcquisition.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * SkillAcquisition — learn reusable skills from experience
 * Part of Phase 5: Continuous Learning & Adaptation
 * Part of the AGENT-ZERO-GENESIS project
 */

#ifndef _OPENCOG_AGENTZERO_SKILL_ACQUISITION_H
#define _OPENCOG_AGENTZERO_SKILL_ACQUISITION_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/ExperienceManager.h"

namespace opencog {
namespace agentzero {

/**
 * Proficiency level of an acquired skill
 */
enum class SkillProficiency {
    NOVICE,       ///< Newly acquired — low success rate
    BEGINNER,     ///< Basic competency established
    INTERMEDIATE, ///< Reliable in familiar contexts
    ADVANCED,     ///< High success rate across contexts
    EXPERT        ///< Near-optimal performance
};

/**
 * A reusable skill extracted from agent experience
 */
struct Skill {
    Handle id;                          ///< Unique identifier atom
    std::string name;                   ///< Human-readable name
    std::string description;            ///< What the skill does
    std::vector<Handle> preconditions;  ///< Atoms that must hold before execution
    std::vector<Handle> effects;        ///< Atoms typically produced by the skill
    std::vector<Handle> action_sequence;///< Prototypical action sequence
    SkillProficiency proficiency;       ///< Current proficiency level
    double success_rate;                ///< Fraction of successful applications
    double transfer_score;              ///< How well the skill transfers to new contexts
    int application_count;              ///< Total number of times applied
    std::map<std::string, Handle> context_variants; ///< Context-specific adaptations

    Skill()
        : proficiency(SkillProficiency::NOVICE)
        , success_rate(0.0)
        , transfer_score(0.0)
        , application_count(0)
    {}
};

/**
 * Configuration for the skill acquisition process
 */
struct SkillAcquisitionConfig {
    double min_success_rate;          ///< Minimum success rate to retain a skill
    int min_demonstrations;           ///< Demonstrations required before generalising
    double similarity_threshold;      ///< Threshold for merging similar skills
    bool enable_skill_transfer;       ///< Allow applying skills in new contexts
    bool enable_incremental_learning; ///< Refine skills from each new experience

    SkillAcquisitionConfig()
        : min_success_rate(0.5)
        , min_demonstrations(3)
        , similarity_threshold(0.7)
        , enable_skill_transfer(true)
        , enable_incremental_learning(true)
    {}
};

/**
 * SkillAcquisition — extracts and refines reusable skills from experience
 *
 * Analyses the agent's experience store to discover repeatable patterns
 * of successful behaviour, generalises them into transferable skills, and
 * refines those skills as more evidence accumulates.
 *
 * Key features:
 * - Skill extraction from successful experience trajectories
 * - Incremental skill refinement with each new episode
 * - Skill transfer and context adaptation
 * - AtomSpace-backed persistent skill library
 */
class SkillAcquisition
{
public:
    /**
     * Construct a SkillAcquisition component.
     * @param atomspace AtomSpace used for skill storage.
     * @param config    Acquisition configuration (optional).
     */
    explicit SkillAcquisition(AtomSpacePtr atomspace,
                              const SkillAcquisitionConfig& config = SkillAcquisitionConfig());

    virtual ~SkillAcquisition();

    // ----------------------------------------------------------------
    // Skill extraction
    // ----------------------------------------------------------------

    /**
     * Attempt to learn new skills from a collection of experiences.
     * Experiences with ExperienceType::SUCCESS are analysed for reusable patterns.
     * @param experiences  Candidate experience records.
     * @return Handles of newly created skill atoms.
     */
    std::vector<Handle> learnFromExperiences(const std::vector<Experience>& experiences);

    /**
     * Update an existing skill given a new application outcome.
     * @param skill_handle  Handle of the skill to refine.
     * @param success       Whether the application was successful.
     * @param context       Context atom at the time of application.
     */
    void refineSkill(const Handle& skill_handle, bool success, const Handle& context);

    // ----------------------------------------------------------------
    // Skill retrieval
    // ----------------------------------------------------------------

    /**
     * Retrieve all skills applicable in the given context.
     * @param context     Current context atom.
     * @param max_results Upper bound on results.
     * @return Applicable skills sorted by proficiency (descending).
     */
    std::vector<Skill> getApplicableSkills(const Handle& context,
                                           int max_results = 10) const;

    /**
     * Retrieve a skill by its atom handle.
     * @param skill_handle Handle returned by learnFromExperiences().
     * @return The Skill record, or a default-constructed Skill on failure.
     */
    Skill getSkill(const Handle& skill_handle) const;

    /** Return all stored skills. */
    std::vector<Skill> getAllSkills() const;

    // ----------------------------------------------------------------
    // Transfer learning
    // ----------------------------------------------------------------

    /**
     * Attempt to adapt a known skill to a new context.
     * @param skill_handle    Source skill.
     * @param new_context     Target context atom.
     * @return Handle of the adapted skill, or Handle::UNDEFINED on failure.
     */
    Handle transferSkill(const Handle& skill_handle, const Handle& new_context);

    // ----------------------------------------------------------------
    // Statistics and diagnostics
    // ----------------------------------------------------------------

    /** Return the number of stored skills. */
    size_t getSkillCount() const { return _skills.size(); }

    /**
     * Aggregate statistics about the skill library.
     * @return Key/value map of statistic names to values.
     */
    std::map<std::string, double> getSkillStatistics() const;

    // ----------------------------------------------------------------
    // Configuration
    // ----------------------------------------------------------------
    void setConfig(const SkillAcquisitionConfig& config) { _config = config; }
    const SkillAcquisitionConfig& getConfig() const { return _config; }

private:
    AtomSpacePtr            _atomspace;
    SkillAcquisitionConfig  _config;

    std::map<Handle, Skill> _skills;        ///< Handle → Skill lookup
    Handle                  _skill_base;    ///< Root concept in AtomSpace

    // Internal helpers
    void   initializeSkillBase();
    Skill  extractSkillFromExperiences(const std::vector<Experience>& related);
    Handle createSkillAtom(const Skill& skill);
    bool   isSimilarSkill(const Skill& a, const Skill& b) const;
    void   mergeSkills(Skill& existing, const Skill& candidate);
    void   updateProficiency(Skill& skill);
    bool   contextMatches(const Skill& skill, const Handle& context) const;
    static std::string proficiencyToString(SkillProficiency p);
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_SKILL_ACQUISITION_H
