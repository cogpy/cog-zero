/*
 * opencog/agentzero/ExperienceManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ExperienceManager — Episodic memory for agent trajectories
 * Part of Phase 5: Continuous Learning & Adaptation
 * Part of the AGENT-ZERO-GENESIS project
 */

#ifndef _OPENCOG_AGENTZERO_EXPERIENCE_MANAGER_H
#define _OPENCOG_AGENTZERO_EXPERIENCE_MANAGER_H

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

/**
 * Types of experiences that can be recorded and retrieved
 */
enum class ExperienceType {
    ACTION_OUTCOME,    ///< Results of actions taken
    OBSERVATION,       ///< Sensory observations
    INTERACTION,       ///< Social interactions
    PROBLEM_SOLVING,   ///< Problem-solving episodes
    SUCCESS,           ///< Successful task completions
    FAILURE,           ///< Failed attempts and errors
    DISCOVERY          ///< New knowledge discoveries
};

/**
 * A single experience record representing one agent trajectory step
 */
struct Experience {
    Handle id;                                         ///< Unique identifier atom
    ExperienceType type;                               ///< Category of experience
    Handle context;                                    ///< Context where it occurred
    Handle task;                                       ///< Associated task or goal
    Handle outcome;                                    ///< Result or outcome atom
    std::chrono::system_clock::time_point timestamp;   ///< When it occurred
    double importance;                                 ///< Importance score [0,1]
    std::map<std::string, Handle> metadata;            ///< Extra key/value metadata

    Experience() : type(ExperienceType::OBSERVATION), importance(0.5) {}
};

/**
 * Query parameters for retrieving experiences
 */
struct ExperienceQuery {
    ExperienceType type_filter;                            ///< Filter by type
    Handle context_filter;                                 ///< Filter by context atom
    Handle task_filter;                                    ///< Filter by task atom
    std::chrono::system_clock::time_point start_time;      ///< Earliest timestamp
    std::chrono::system_clock::time_point end_time;        ///< Latest timestamp
    double min_importance;                                 ///< Minimum importance
    int max_results;                                       ///< Maximum result count

    ExperienceQuery()
        : type_filter(ExperienceType::OBSERVATION)
        , min_importance(0.0)
        , max_results(100)
    {}
};

/**
 * ExperienceManager — manages the agent's episodic memory of trajectories
 *
 * Stores, retrieves and organises agent experiences in the AtomSpace.
 * Experiences accumulate during operation and are used for learning,
 * skill acquisition, and policy optimisation.
 *
 * Key features:
 * - AtomSpace-backed persistent experience storage
 * - Type-based and context-based experience retrieval
 * - Importance-weighted memory consolidation
 * - Temporal indexing for chronological access
 */
class ExperienceManager
{
public:
    /**
     * Construct an ExperienceManager.
     * @param atomspace          AtomSpace for persistent storage.
     * @param max_experiences    Maximum number of experiences to retain.
     * @param importance_threshold  Minimum importance score kept during consolidation.
     * @param retention_period   How long to retain experiences before expiry.
     */
    ExperienceManager(AtomSpacePtr atomspace,
                      size_t max_experiences = 10000,
                      double importance_threshold = 0.1,
                      std::chrono::hours retention_period = std::chrono::hours(24 * 30));

    virtual ~ExperienceManager();

    // ----------------------------------------------------------------
    // Recording
    // ----------------------------------------------------------------

    /**
     * Record a new experience in episodic memory.
     * @param type        Category of this experience.
     * @param context     Contextual atom at the time of the experience.
     * @param task        Task or goal atom associated with this experience.
     * @param outcome     Result/outcome atom.
     * @param importance  Importance score in [0,1].
     * @return Handle identifying the stored experience, or Handle::UNDEFINED on error.
     */
    Handle recordExperience(ExperienceType type,
                            const Handle& context,
                            const Handle& task,
                            const Handle& outcome,
                            double importance = 0.5);

    // ----------------------------------------------------------------
    // Retrieval
    // ----------------------------------------------------------------

    /**
     * Query experiences using structured criteria.
     * @param query  Query parameters.
     * @return Matching experiences (up to query.max_results).
     */
    std::vector<Experience> queryExperiences(const ExperienceQuery& query) const;

    /**
     * Get all experiences of a given type.
     * @param type       Experience category.
     * @param max_count  Upper bound on results (0 = unlimited).
     * @return Matching experiences.
     */
    std::vector<Experience> getExperiencesByType(ExperienceType type,
                                                 int max_count = 100) const;

    /**
     * Get the most recent experiences.
     * @param time_window  How far back to look.
     * @param max_count    Upper bound on results.
     * @return Recent experiences in reverse chronological order.
     */
    std::vector<Experience> getRecentExperiences(std::chrono::hours time_window,
                                                 int max_count = 100) const;

    /**
     * Get experiences associated with a specific context atom.
     * @param context    Context atom to match.
     * @param max_count  Upper bound on results.
     * @return Matching experiences.
     */
    std::vector<Experience> getExperiencesByContext(const Handle& context,
                                                    int max_count = 100) const;

    /**
     * Find experiences similar to a given target.
     * Similarity is currently based on shared context and task atoms.
     * @param target      Experience to compare against.
     * @param max_results Upper bound on results.
     * @return Similar experiences sorted by descending similarity.
     */
    std::vector<Experience> findSimilarExperiences(const Experience& target,
                                                   int max_results = 10) const;

    // ----------------------------------------------------------------
    // Importance / memory management
    // ----------------------------------------------------------------

    /**
     * Update the importance score of an experience.
     * @param experience_handle  Handle returned by recordExperience().
     * @param new_importance     New score in [0,1].
     * @return True if the experience was found and updated.
     */
    bool updateExperienceImportance(const Handle& experience_handle,
                                    double new_importance);

    /**
     * Manually trigger memory consolidation.
     * Removes expired or low-importance experiences until the stored
     * count falls below max_experiences.
     */
    void consolidateMemory();

    /** Remove every stored experience. */
    void clearAllExperiences();

    // ----------------------------------------------------------------
    // Statistics and diagnostics
    // ----------------------------------------------------------------

    /** Return the number of stored experiences. */
    size_t getExperienceCount() const { return _experiences.size(); }

    /**
     * Aggregate statistics (counts by type, average importance, etc.).
     * @return Key/value map of statistic names to values.
     */
    std::map<std::string, double> getExperienceStatistics() const;

    /** @return True once the AtomSpace structures have been initialised. */
    bool isInitialized() const { return _initialized; }

    // ----------------------------------------------------------------
    // Configuration
    // ----------------------------------------------------------------
    void setMaxExperiences(size_t max_experiences);
    void setImportanceThreshold(double threshold);
    void setRetentionPeriod(std::chrono::hours period);

    // ----------------------------------------------------------------
    // Utility
    // ----------------------------------------------------------------
    static std::string experienceTypeToString(ExperienceType type);
    static ExperienceType stringToExperienceType(const std::string& type_str);

private:
    // Core references
    AtomSpacePtr _atomspace;

    // Experience storage
    std::vector<Experience>                             _experiences;
    std::map<Handle, size_t>                            _experience_index;
    std::map<ExperienceType, std::vector<size_t>>       _type_index;
    std::map<Handle, std::vector<size_t>>               _context_index;

    // Memory management configuration
    size_t              _max_experiences;
    double              _importance_threshold;
    std::chrono::hours  _retention_period;

    // AtomSpace housekeeping handles
    Handle _experience_context;
    Handle _memory_link;
    bool   _initialized;

    // Internal helpers
    void   initialize();
    Handle createExperienceAtom(const Experience& exp);
    void   indexExperience(const Experience& exp, size_t index);
    void   removeExperienceFromIndices(size_t index);
    bool   shouldRetainExperience(const Experience& exp) const;
    static double calculateSimilarity(const Experience& a, const Experience& b);
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_EXPERIENCE_MANAGER_H
