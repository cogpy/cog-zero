/**
 * LearningTypes.h - Common types and constants for Agent-Zero Learning Module
 * 
 * Part of AZ-LEARN-003: MOSES Policy Optimization Integration
 * Implements Phase 5: Learning & Adaptation
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#ifndef AGENTZERO_LEARNING_TYPES_H
#define AGENTZERO_LEARNING_TYPES_H

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>

#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

// MOSES integration headers
#include <moses/moses/moses/moses_main.h>
#include <moses/comboreduct/combo/combo.h>

// ASMOSES integration headers  
#include <opencog/asmoses/moses/main/moses_exec.h>
#include <opencog/asmoses/moses/main/populate_atomspace.h>

namespace opencog {
namespace agentzero {
namespace learning {

// Forward declarations
class ExperienceManager;
class PolicyOptimizer;
class SkillAcquisition;
class MetaLearning;

// Type aliases
using AtomSpacePtr = std::shared_ptr<AtomSpace>;
using HandleSeq = std::vector<Handle>;
using HandleSet = std::set<Handle>;
using PolicyId = std::string;
using ExperienceId = std::string;
using SkillId = std::string;

/**
 * Learning configuration parameters
 */
struct LearningConfig {
    // MOSES parameters
    size_t max_evals = 10000;          // Maximum evaluations for MOSES
    size_t max_gens = 1000;            // Maximum generations
    double diversity_pressure = 0.1;    // Diversity pressure for population
    size_t population_size = 500;       // Population size
    
    // Policy optimization parameters
    double learning_rate = 0.01;        // Learning rate for policy updates
    double exploration_rate = 0.1;      // Exploration vs exploitation
    size_t experience_buffer_size = 1000; // Size of experience replay buffer
    
    // Skill acquisition parameters
    size_t skill_complexity_threshold = 10; // Minimum complexity for new skills
    double skill_success_threshold = 0.8;   // Success rate for skill validation
    
    // Meta-learning parameters
    size_t meta_batch_size = 32;        // Batch size for meta-learning
    double meta_learning_rate = 0.001;  // Meta-learning rate
    
    // AtomSpace integration
    std::string policy_atom_prefix = "PolicyAtom_";
    std::string experience_atom_prefix = "ExperienceAtom_";
    std::string skill_atom_prefix = "SkillAtom_";
};

/**
 * Advanced experience classification from comprehensive experience management
 */
enum class ExperienceType {
    ACTION_OUTCOME,     // Results of actions taken
    INTERACTION,        // Social/environmental interactions
    PROBLEM_SOLVING,    // Problem-solving episodes
    SKILL_APPLICATION,  // Application of learned skills
    GOAL_PURSUIT,       // Goal-directed behavior sequences
    UNEXPECTED,         // Unexpected events or outcomes
    LEARNING_EPISODE,   // Meta-learning experiences
    EMOTIONAL           // Emotional experiences and reactions
};

// Experience outcome classification
enum class ExperienceOutcome {
    SUCCESS,            // Experience led to desired outcome
    FAILURE,            // Experience did not achieve goal
    PARTIAL_SUCCESS,    // Partially achieved desired outcome
    UNEXPECTED_OUTCOME, // Outcome was different than expected
    INCONCLUSIVE,       // Outcome unclear or mixed
    LEARNING_OPPORTUNITY // Experience provided valuable learning
};

// Experience importance levels
enum class ExperienceImportance {
    CRITICAL = 100,     // Critical experiences for survival/success
    HIGH = 75,         // High importance experiences
    MEDIUM = 50,       // Medium importance experiences
    LOW = 25,          // Low importance experiences
    ROUTINE = 10       // Routine, common experiences
};

/**
 * Comprehensive experience data structure combining both approaches
 */
struct Experience {
    ExperienceId id;
    Handle state_atom;        // Current state representation
    Handle action_atom;       // Action taken
    Handle next_state_atom;   // Resulting state
    double reward;            // Reward received
    bool terminal;            // Whether this is a terminal state
    uint64_t timestamp;       // When this experience occurred
    
    // Advanced classification (from main branch)
    ExperienceType experience_type = ExperienceType::ACTION_OUTCOME;
    ExperienceOutcome outcome = ExperienceOutcome::INCONCLUSIVE;
    ExperienceImportance importance = ExperienceImportance::MEDIUM;
    double confidence_level = 0.5;  // Confidence in the experience classification
    
    // Additional context
    std::map<std::string, Handle> context_atoms;
    std::map<std::string, double> numeric_features;
    std::vector<Handle> environmental_state;  // Environmental context atoms
    std::vector<Handle> agent_state;          // Agent internal state atoms
    
    Experience() : reward(0.0), terminal(false), timestamp(0) {}
    
    Experience(const ExperienceId& exp_id, Handle state, Handle action,
               Handle next_state, double r, bool term, uint64_t ts)
        : id(exp_id), state_atom(state), action_atom(action),
          next_state_atom(next_state), reward(r), terminal(term), timestamp(ts) {}
    
    // Constructor with advanced classification
    Experience(const ExperienceId& exp_id, Handle state, Handle action,
               Handle next_state, double r, bool term, uint64_t ts,
               ExperienceType type, ExperienceOutcome out, ExperienceImportance imp)
        : id(exp_id), state_atom(state), action_atom(action),
          next_state_atom(next_state), reward(r), terminal(term), timestamp(ts),
          experience_type(type), outcome(out), importance(imp) {}
};

/**
 * Policy representation combining MOSES-evolved programs with AtomSpace
 */
struct Policy {
    PolicyId id;
    Handle policy_atom;                    // AtomSpace representation
    opencog::combo::combo_tree program;   // MOSES-evolved program
    double fitness_score;                  // Current fitness/performance
    size_t evaluation_count;               // Number of evaluations
    std::string program_source;            // Source code representation
    
    // Policy metadata
    std::vector<std::string> input_features;  // Expected input features
    std::string output_type;                  // Type of output (action, value, etc.)
    std::map<std::string, double> performance_metrics;
    
    Policy() : fitness_score(0.0), evaluation_count(0) {}
    
    Policy(const PolicyId& pol_id, Handle atom, const opencog::combo::combo_tree& prog)
        : id(pol_id), policy_atom(atom), program(prog), fitness_score(0.0), evaluation_count(0) {}
};

/**
 * Skill representation for hierarchical skill acquisition
 */
struct Skill {
    SkillId id;
    Handle skill_atom;                // AtomSpace representation
    std::vector<PolicyId> sub_policies; // Constituent policies
    std::string skill_name;           // Human-readable name
    std::string description;          // Skill description
    
    // Skill performance metrics
    double success_rate;              // Success rate in applications
    double complexity_score;          // Estimated complexity
    size_t usage_count;              // How often this skill is used
    
    // Hierarchical structure
    std::vector<SkillId> prerequisite_skills;  // Required prerequisite skills
    std::vector<SkillId> derived_skills;       // Skills that build on this one
    
    Skill() : success_rate(0.0), complexity_score(0.0), usage_count(0) {}
    
    Skill(const SkillId& sk_id, Handle atom, const std::string& name)
        : id(sk_id), skill_atom(atom), skill_name(name), 
          success_rate(0.0), complexity_score(0.0), usage_count(0) {}
};

/**
 * Fitness function interface for MOSES policy optimization
 */
class PolicyFitnessFunction {
public:
    virtual ~PolicyFitnessFunction() = default;
    
    /**
     * Evaluate the fitness of a given policy program
     * @param program The MOSES-evolved combo program
     * @param context Optional context atoms for evaluation
     * @return Fitness score (higher is better)
     */
    virtual double evaluate(const opencog::combo::combo_tree& program,
                          const std::map<std::string, Handle>& context = {}) = 0;
    
    /**
     * Get the name/description of this fitness function
     */
    virtual std::string getName() const = 0;
    
    /**
     * Get the expected input features for policies evaluated by this function
     */
    virtual std::vector<std::string> getInputFeatures() const = 0;
};

/**
 * Callback interface for learning events
 */
class LearningEventCallback {
public:
    virtual ~LearningEventCallback() = default;
    
    virtual void onPolicyEvolved(const Policy& policy) {}
    virtual void onExperienceAdded(const Experience& experience) {}
    virtual void onSkillAcquired(const Skill& skill) {}
    virtual void onLearningIteration(size_t iteration, double performance) {}
};

/**
 * Utility functions and constants
 */
namespace constants {
    // AtomSpace node types for learning
    const std::string POLICY_NODE_TYPE = "PolicyNode";
    const std::string EXPERIENCE_NODE_TYPE = "ExperienceNode";
    const std::string SKILL_NODE_TYPE = "SkillNode";
    
    // Link types for relationships
    const std::string POLICY_EVALUATION_LINK = "PolicyEvaluationLink";
    const std::string EXPERIENCE_SEQUENCE_LINK = "ExperienceSequenceLink";
    const std::string SKILL_HIERARCHY_LINK = "SkillHierarchyLink";
    
    // Value keys for storing data
    const std::string FITNESS_VALUE_KEY = "fitness";
    const std::string PROGRAM_VALUE_KEY = "program";
    const std::string METRICS_VALUE_KEY = "metrics";
    
    // Default configuration values
    const size_t DEFAULT_POPULATION_SIZE = 500;
    const size_t DEFAULT_MAX_EVALS = 10000;
    const double DEFAULT_DIVERSITY_PRESSURE = 0.1;
}

/**
 * Exception classes for learning module
 */
class LearningException : public std::runtime_error {
public:
    explicit LearningException(const std::string& msg) : std::runtime_error(msg) {}
};

class MOSESIntegrationException : public LearningException {
public:
    explicit MOSESIntegrationException(const std::string& msg) 
        : LearningException("MOSES Integration Error: " + msg) {}
};

class PolicyOptimizationException : public LearningException {
public:
    explicit PolicyOptimizationException(const std::string& msg)
        : LearningException("Policy Optimization Error: " + msg) {}
};

class SkillAcquisitionException : public LearningException {
public:
    explicit SkillAcquisitionException(const std::string& msg)
        : LearningException("Skill Acquisition Error: " + msg) {}
};

} // namespace learning
} // namespace agentzero
} // namespace opencog

#endif // AGENTZERO_LEARNING_TYPES_H