/**
 * LearningUtils.cpp - Implementation of utility functions for Agent-Zero Learning
 * 
 * Part of AZ-LEARN-003: MOSES Policy Optimization Integration
 * Copyright (C) 2024 OpenCog Foundation
 */

#include <agentzero/learning/LearningUtils.h>

#include <opencog/util/Logger.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <ctime>
#include <random>

// MOSES includes for combo tree utilities (optional)
#ifdef HAVE_MOSES
#include <moses/comboreduct/combo/combo.h>
#include <moses/comboreduct/combo/iostream_combo.h>
#endif

namespace opencog {
namespace agentzero {
namespace learning {
namespace utils {

std::string generateUniqueId(const std::string& prefix) {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::ostringstream oss;
    oss << prefix << millis << "_" << dis(gen);
    return oss.str();
}

uint64_t getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// RandomGenerator implementation
RandomGenerator& RandomGenerator::getInstance() {
    static RandomGenerator instance;
    return instance;
}

RandomGenerator::RandomGenerator() : uniform_dist_(0.0, 1.0) {
    std::random_device rd;
    generator_.seed(rd());
}

double RandomGenerator::uniform(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(generator_);
}

int RandomGenerator::uniformInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(generator_);
}

size_t RandomGenerator::uniformSize(size_t min, size_t max) {
    std::uniform_int_distribution<size_t> dist(min, max);
    return dist(generator_);
}

bool RandomGenerator::bernoulli(double probability) {
    std::bernoulli_distribution dist(probability);
    return dist(generator_);
}

void RandomGenerator::seed(unsigned int seed_value) {
    generator_.seed(seed_value);
}

// Statistical utilities
double mean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / values.size();
}

double standardDeviation(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;
    
    double avg = mean(values);
    double variance = 0.0;
    
    for (double value : values) {
        variance += (value - avg) * (value - avg);
    }
    
    variance /= (values.size() - 1);
    return std::sqrt(variance);
}

double median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    
    std::sort(values.begin(), values.end());
    
    size_t n = values.size();
    if (n % 2 == 0) {
        return (values[n/2 - 1] + values[n/2]) / 2.0;
    } else {
        return values[n/2];
    }
}

// String utilities
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    
    return tokens;
}

std::string join(const std::vector<std::string>& strings, const std::string& delimiter) {
    if (strings.empty()) return "";
    
    std::ostringstream oss;
    oss << strings[0];
    
    for (size_t i = 1; i < strings.size(); ++i) {
        oss << delimiter << strings[i];
    }
    
    return oss.str();
}

// AtomSpace utilities
Handle createLearningNode(AtomSpacePtr atomspace, const std::string& node_type, const std::string& name) {
    if (!atomspace) {
        logger().error("LearningUtils: Cannot create node with null AtomSpace");
        return Handle::UNDEFINED;
    }
    
    try {
        Type type = nameserver().getType(node_type);
        if (type == NOTYPE) {
            logger().error("LearningUtils: Unknown node type: %s", node_type.c_str());
            return Handle::UNDEFINED;
        }
        
        return atomspace->add_node(type, name);
    } catch (const std::exception& e) {
        logger().error("LearningUtils: Error creating node: %s", e.what());
        return Handle::UNDEFINED;
    }
}

Handle createLearningLink(AtomSpacePtr atomspace, const std::string& link_type, const HandleSeq& outgoing) {
    if (!atomspace) {
        logger().error("LearningUtils: Cannot create link with null AtomSpace");
        return Handle::UNDEFINED;
    }
    
    try {
        Type type = nameserver().getType(link_type);
        if (type == NOTYPE) {
            logger().error("LearningUtils: Unknown link type: %s", link_type.c_str());
            return Handle::UNDEFINED;
        }
        
        return atomspace->add_link(type, outgoing);
    } catch (const std::exception& e) {
        logger().error("LearningUtils: Error creating link: %s", e.what());
        return Handle::UNDEFINED;
    }
}

// MOSES integration utilities
#ifdef HAVE_MOSES
std::string comboTreeToString(const opencog::combo::combo_tree& tree) {
    try {
        std::ostringstream oss;
        oss << tree;
        return oss.str();
    } catch (const std::exception& e) {
        logger().error("LearningUtils: Error converting combo tree to string: %s", e.what());
        return "";
    }
}

opencog::combo::combo_tree stringToComboTree(const std::string& tree_string) {
    try {
        std::istringstream iss(tree_string);
        opencog::combo::combo_tree tree;
        iss >> tree;
        return tree;
    } catch (const std::exception& e) {
        logger().error("LearningUtils: Error converting string to combo tree: %s", e.what());
        return opencog::combo::combo_tree();
    }
}
#endif // HAVE_MOSES

// Configuration utilities
LearningConfig loadConfigFromFile(const std::string& config_file) {
    LearningConfig config;
    
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            logger().warn("LearningUtils: Could not open config file: %s. Using defaults.", config_file.c_str());
            return config;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            
            auto parts = split(line, '=');
            if (parts.size() != 2) continue;
            
            std::string key = trim(parts[0]);
            std::string value = trim(parts[1]);
            
            // Parse configuration values
            if (key == "max_evals") {
                config.max_evals = std::stoul(value);
            } else if (key == "max_gens") {
                config.max_gens = std::stoul(value);
            } else if (key == "diversity_pressure") {
                config.diversity_pressure = std::stod(value);
            } else if (key == "population_size") {
                config.population_size = std::stoul(value);
            } else if (key == "learning_rate") {
                config.learning_rate = std::stod(value);
            } else if (key == "exploration_rate") {
                config.exploration_rate = std::stod(value);
            } else if (key == "experience_buffer_size") {
                config.experience_buffer_size = std::stoul(value);
            } else if (key == "skill_complexity_threshold") {
                config.skill_complexity_threshold = std::stoul(value);
            } else if (key == "skill_success_threshold") {
                config.skill_success_threshold = std::stod(value);
            } else if (key == "meta_batch_size") {
                config.meta_batch_size = std::stoul(value);
            } else if (key == "meta_learning_rate") {
                config.meta_learning_rate = std::stod(value);
            } else if (key == "policy_atom_prefix") {
                config.policy_atom_prefix = value;
            } else if (key == "experience_atom_prefix") {
                config.experience_atom_prefix = value;
            } else if (key == "skill_atom_prefix") {
                config.skill_atom_prefix = value;
            }
        }
        
        logger().info("LearningUtils: Loaded configuration from: %s", config_file.c_str());
        
    } catch (const std::exception& e) {
        logger().error("LearningUtils: Error loading configuration: %s", e.what());
    }
    
    return config;
}

void saveConfigToFile(const LearningConfig& config, const std::string& config_file) {
    try {
        std::ofstream file(config_file);
        if (!file.is_open()) {
            logger().error("LearningUtils: Could not open config file for writing: %s", config_file.c_str());
            return;
        }
        
        file << "# Agent-Zero Learning Configuration\n";
        file << "# Generated on " << std::ctime(nullptr) << "\n\n";
        
        file << "# MOSES parameters\n";
        file << "max_evals=" << config.max_evals << "\n";
        file << "max_gens=" << config.max_gens << "\n";
        file << "diversity_pressure=" << config.diversity_pressure << "\n";
        file << "population_size=" << config.population_size << "\n\n";
        
        file << "# Policy optimization parameters\n";
        file << "learning_rate=" << config.learning_rate << "\n";
        file << "exploration_rate=" << config.exploration_rate << "\n";
        file << "experience_buffer_size=" << config.experience_buffer_size << "\n\n";
        
        file << "# Skill acquisition parameters\n";
        file << "skill_complexity_threshold=" << config.skill_complexity_threshold << "\n";
        file << "skill_success_threshold=" << config.skill_success_threshold << "\n\n";
        
        file << "# Meta-learning parameters\n";
        file << "meta_batch_size=" << config.meta_batch_size << "\n";
        file << "meta_learning_rate=" << config.meta_learning_rate << "\n\n";
        
        file << "# AtomSpace integration\n";
        file << "policy_atom_prefix=" << config.policy_atom_prefix << "\n";
        file << "experience_atom_prefix=" << config.experience_atom_prefix << "\n";
        file << "skill_atom_prefix=" << config.skill_atom_prefix << "\n";
        
        logger().info("LearningUtils: Saved configuration to: %s", config_file.c_str());
        
    } catch (const std::exception& e) {
        logger().error("LearningUtils: Error saving configuration: %s", e.what());
    }
}

LearningConfig getDefaultConfig(const std::string& preset) {
    LearningConfig config;
    
    if (preset == "fast") {
        // Fast configuration for testing
        config.max_evals = 1000;
        config.max_gens = 100;
        config.population_size = 100;
        config.experience_buffer_size = 500;
    } else if (preset == "thorough") {
        // Thorough configuration for production
        config.max_evals = 50000;
        config.max_gens = 5000;
        config.population_size = 1000;
        config.experience_buffer_size = 5000;
    } else if (preset == "memory_efficient") {
        // Memory efficient configuration
        config.max_evals = 5000;
        config.max_gens = 500;
        config.population_size = 200;
        config.experience_buffer_size = 1000;
    }
    // "default" preset uses the values from LearningConfig constructor
    
    return config;
}

// Logging utilities
void logLearningEvent(const std::string& event, const std::map<std::string, std::string>& details) {
    std::ostringstream oss;
    oss << "Learning Event: " << event;
    
    if (!details.empty()) {
        oss << " [";
        bool first = true;
        for (const auto& pair : details) {
            if (!first) oss << ", ";
            oss << pair.first << "=" << pair.second;
            first = false;
        }
        oss << "]";
    }
    
    logger().info("%s", oss.str().c_str());
}

void logPolicyEvolution(const PolicyId& policy_id, size_t generation, double fitness) {
    logger().info("Policy Evolution: %s - Gen %zu, Fitness %.4f", 
                  policy_id.c_str(), generation, fitness);
}

void logExperienceUpdate(const ExperienceId& exp_id, const std::string& event_type) {
    logger().debug("Experience Update: %s - %s", exp_id.c_str(), event_type.c_str());
}

} // namespace utils
} // namespace learning
} // namespace agentzero
} // namespace opencog