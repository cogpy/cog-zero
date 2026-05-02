/**
 * LearningUtils.h - Utility functions for Agent-Zero Learning Module
 * 
 * Part of AZ-LEARN-003: MOSES Policy Optimization Integration
 * Provides common utility functions for learning components
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#ifndef AGENTZERO_LEARNING_UTILS_H
#define AGENTZERO_LEARNING_UTILS_H

#include <string>
#include <vector>
#include <map>
#include <random>
#include <chrono>

#include "LearningTypes.h"

namespace opencog {
namespace agentzero {
namespace learning {
namespace utils {

/**
 * Generate unique identifiers
 */
std::string generateUniqueId(const std::string& prefix = "");

/**
 * Current timestamp in milliseconds
 */
uint64_t getCurrentTimestamp();

/**
 * Random number utilities
 */
class RandomGenerator {
public:
    static RandomGenerator& getInstance();
    
    double uniform(double min = 0.0, double max = 1.0);
    int uniformInt(int min, int max);
    size_t uniformSize(size_t min, size_t max);
    bool bernoulli(double probability = 0.5);
    
    template<typename T>
    T choice(const std::vector<T>& options) {
        if (options.empty()) throw std::invalid_argument("Empty options vector");
        return options[uniformSize(0, options.size() - 1)];
    }
    
    void seed(unsigned int seed_value);
    
    // Public access to generator for std algorithms
    std::mt19937& getGenerator() { return generator_; }

private:
    RandomGenerator();
    std::mt19937 generator_;
    std::uniform_real_distribution<double> uniform_dist_;
};

/**
 * Statistical utilities
 */
double mean(const std::vector<double>& values);
double standardDeviation(const std::vector<double>& values);
double median(std::vector<double> values);  // Note: modifies input

/**
 * String utilities
 */
std::string trim(const std::string& str);
std::vector<std::string> split(const std::string& str, char delimiter);
std::string join(const std::vector<std::string>& strings, const std::string& delimiter);

/**
 * AtomSpace utilities for learning
 */
Handle createLearningNode(AtomSpacePtr atomspace, const std::string& node_type, const std::string& name);
Handle createLearningLink(AtomSpacePtr atomspace, const std::string& link_type, const HandleSeq& outgoing);

/**
 * MOSES integration utilities
 */
#ifdef HAVE_MOSES
#include <moses/comboreduct/combo/combo.h>
std::string comboTreeToString(const opencog::combo::combo_tree& tree);
opencog::combo::combo_tree stringToComboTree(const std::string& tree_string);
#endif // HAVE_MOSES

/**
 * Configuration utilities
 */
LearningConfig loadConfigFromFile(const std::string& config_file);
void saveConfigToFile(const LearningConfig& config, const std::string& config_file);
LearningConfig getDefaultConfig(const std::string& preset = "default");

/**
 * Logging utilities
 */
void logLearningEvent(const std::string& event, const std::map<std::string, std::string>& details = {});
void logPolicyEvolution(const PolicyId& policy_id, size_t generation, double fitness);
void logExperienceUpdate(const ExperienceId& exp_id, const std::string& event_type);

} // namespace utils
} // namespace learning
} // namespace agentzero
} // namespace opencog

#endif // AGENTZERO_LEARNING_UTILS_H