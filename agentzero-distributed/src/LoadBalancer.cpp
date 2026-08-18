/**
 * LoadBalancer.cpp
 * 
 * Implementation of LoadBalancer for Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 */

#include <opencog/agentzero/distributed/LoadBalancer.h>
#include <opencog/agentzero/distributed/DistributedCoordinator.h>
#include <opencog/atoms/base/Node.h>
#include <algorithm>
#include <random>
#include <cmath>
#include <climits>

using namespace opencog;
using namespace opencog::agentzero;

// Configuration constants
namespace {
    // Threshold for suggesting load rebalancing (percentage difference)
    constexpr int REBALANCE_THRESHOLD = 20;
}

LoadBalancer::LoadBalancer(AtomSpacePtr atomspace, LoadBalancingStrategy strategy)
    : atomspace_(atomspace),
      strategy_(strategy),
      adaptive_mode_(false),
      round_robin_index_(0),
      logger_("LoadBalancer")
{
    logger_.info("LoadBalancer created with strategy: %d", static_cast<int>(strategy));
}

LoadBalancer::~LoadBalancer()
{
    logger_.info("LoadBalancer destroyed");
}

TaskAssignment LoadBalancer::assignTask(const DistributedTask& task,
                                       const std::vector<ComputeNode>& available_nodes)
{
    if (available_nodes.empty()) {
        logger_.error("No available nodes for task assignment");
        TaskAssignment result;
        result.task_id = task.task_id;
        result.success = false;
        result.error_message = "No available nodes";
        return result;
    }
    
    // Select strategy based on current mode
    TaskAssignment assignment;
    
    switch (strategy_) {
        case LoadBalancingStrategy::ROUND_ROBIN:
            assignment = assignRoundRobin(task, available_nodes);
            break;
        case LoadBalancingStrategy::LEAST_LOADED:
            assignment = assignLeastLoaded(task, available_nodes);
            break;
        case LoadBalancingStrategy::WEIGHTED_RANDOM:
            assignment = assignWeightedRandom(task, available_nodes);
            break;
        case LoadBalancingStrategy::TASK_AFFINITY:
            assignment = assignTaskAffinity(task, available_nodes);
            break;
        case LoadBalancingStrategy::LOCALITY_AWARE:
            assignment = assignLocalityAware(task, available_nodes);
            break;
        default:
            assignment = assignLeastLoaded(task, available_nodes);
    }
    
    // Update statistics
    if (assignment.success) {
        std::lock_guard<std::mutex> lock(load_mutex_);
        assignments_per_node_[assignment.node_id]++;
    }
    
    // Adaptive strategy update
    if (adaptive_mode_) {
        updateStrategy();
    }
    
    return assignment;
}

std::vector<TaskAssignment> LoadBalancer::assignTasks(
    const std::vector<DistributedTask>& tasks,
    const std::vector<ComputeNode>& available_nodes)
{
    std::vector<TaskAssignment> assignments;
    assignments.reserve(tasks.size());
    
    for (const auto& task : tasks) {
        assignments.push_back(assignTask(task, available_nodes));
    }
    
    return assignments;
}

void LoadBalancer::setStrategy(LoadBalancingStrategy strategy)
{
    logger_.info("Changing strategy from %d to %d", 
                static_cast<int>(strategy_), static_cast<int>(strategy));
    strategy_ = strategy;
}

void LoadBalancer::updateNodeLoad(const std::string& node_id, int current_load)
{
    std::lock_guard<std::mutex> lock(load_mutex_);
    node_loads_[node_id] = current_load;
}

std::map<std::string, double> LoadBalancer::getLoadStats() const
{
    std::lock_guard<std::mutex> lock(load_mutex_);
    
    std::map<std::string, double> stats;
    
    if (node_loads_.empty()) {
        return stats;
    }
    
    // Calculate statistics
    double total_load = 0.0;
    double max_load = 0.0;
    double min_load = 100.0;
    
    for (const auto& pair : node_loads_) {
        total_load += pair.second;
        max_load = std::max(max_load, static_cast<double>(pair.second));
        min_load = std::min(min_load, static_cast<double>(pair.second));
    }
    
    double avg_load = total_load / node_loads_.size();
    
    stats["average_load"] = avg_load;
    stats["max_load"] = max_load;
    stats["min_load"] = min_load;
    stats["load_variance"] = max_load - min_load;
    stats["total_assignments"] = static_cast<double>(assignments_per_node_.size());
    
    return stats;
}

std::map<std::string, std::string> LoadBalancer::suggestRebalancing(
    const std::vector<ComputeNode>& nodes)
{
    std::map<std::string, std::string> migrations;
    
    if (nodes.size() < 2) {
        return migrations;  // No rebalancing needed with < 2 nodes
    }
    
    // Find most and least loaded nodes
    const ComputeNode* most_loaded = nullptr;
    const ComputeNode* least_loaded = nullptr;
    
    for (const auto& node : nodes) {
        if (!most_loaded || node.current_load > most_loaded->current_load) {
            most_loaded = &node;
        }
        if (!least_loaded || node.current_load < least_loaded->current_load) {
            least_loaded = &node;
        }
    }
    
    // Suggest migration if imbalance is significant
    if (most_loaded && least_loaded) {
        int load_diff = most_loaded->current_load - least_loaded->current_load;
        if (load_diff > REBALANCE_THRESHOLD) {
            logger_.info("Suggesting rebalancing: move tasks from %s to %s",
                        most_loaded->id.c_str(), least_loaded->id.c_str());
            // In real implementation, would identify specific tasks to migrate
        }
    }
    
    return migrations;
}

std::map<std::string, int> LoadBalancer::calculateDistribution(
    int num_tasks,
    const std::vector<ComputeNode>& nodes)
{
    std::map<std::string, int> distribution;
    
    if (nodes.empty() || num_tasks <= 0) {
        return distribution;
    }
    
    // Calculate total capacity
    int total_capacity = 0;
    for (const auto& node : nodes) {
        if (node.is_active) {
            total_capacity += (node.capacity - node.current_load);
        }
    }
    
    if (total_capacity <= 0) {
        return distribution;
    }
    
    // Distribute proportionally to available capacity
    for (const auto& node : nodes) {
        if (node.is_active) {
            int available = node.capacity - node.current_load;
            int tasks_for_node = (num_tasks * available) / total_capacity;
            distribution[node.id] = tasks_for_node;
        }
    }
    
    return distribution;
}

void LoadBalancer::setAdaptiveMode(bool enabled)
{
    adaptive_mode_ = enabled;
    logger_.info("Adaptive mode %s", enabled ? "enabled" : "disabled");
}

// Private strategy implementations

TaskAssignment LoadBalancer::assignRoundRobin(const DistributedTask& task,
                                             const std::vector<ComputeNode>& nodes)
{
    TaskAssignment assignment;
    assignment.task_id = task.task_id;
    
    if (nodes.empty()) {
        assignment.success = false;
        return assignment;
    }
    
    // Select next node in round-robin fashion
    size_t index = round_robin_index_ % nodes.size();
    round_robin_index_++;
    
    assignment.node_id = nodes[index].id;
    assignment.success = true;
    
    return assignment;
}

TaskAssignment LoadBalancer::assignLeastLoaded(const DistributedTask& task,
                                              const std::vector<ComputeNode>& nodes)
{
    TaskAssignment assignment;
    assignment.task_id = task.task_id;
    
    // Find node with least load
    const ComputeNode* best_node = nullptr;
    int min_load = INT_MAX;
    
    for (const auto& node : nodes) {
        if (node.is_active && node.current_load < min_load) {
            min_load = node.current_load;
            best_node = &node;
        }
    }
    
    if (best_node) {
        assignment.node_id = best_node->id;
        assignment.success = true;
    } else {
        assignment.success = false;
    }
    
    return assignment;
}

TaskAssignment LoadBalancer::assignWeightedRandom(const DistributedTask& task,
                                                 const std::vector<ComputeNode>& nodes)
{
    TaskAssignment assignment;
    assignment.task_id = task.task_id;
    
    // Build weighted distribution based on available capacity
    std::vector<int> weights;
    for (const auto& node : nodes) {
        if (node.is_active) {
            int available = std::max(0, node.capacity - node.current_load);
            weights.push_back(available);
        } else {
            weights.push_back(0);
        }
    }
    
    // Select random node weighted by capacity
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::discrete_distribution<> dist(weights.begin(), weights.end());
    
    int selected_idx = dist(gen);
    assignment.node_id = nodes[selected_idx].id;
    assignment.success = true;
    
    return assignment;
}

TaskAssignment LoadBalancer::assignTaskAffinity(const DistributedTask& task,
                                               const std::vector<ComputeNode>& nodes)
{
    // For task affinity, we'd check node capabilities against task requirements
    // For now, fall back to least loaded
    return assignLeastLoaded(task, nodes);
}

TaskAssignment LoadBalancer::assignLocalityAware(const DistributedTask& task,
                                                const std::vector<ComputeNode>& nodes)
{
    // For locality awareness, we'd check which nodes have relevant data
    // For now, fall back to least loaded
    return assignLeastLoaded(task, nodes);
}

// Helper methods

bool LoadBalancer::nodeSupportsTask(const ComputeNode& node, 
                                   const std::string& task_type)
{
    // In real implementation, would check node capabilities
    return node.is_active;
}

double LoadBalancer::calculateTaskAffinity(const ComputeNode& node, 
                                          const DistributedTask& task)
{
    // Calculate affinity score based on task type and node capabilities
    return 1.0;
}

int LoadBalancer::calculateNodeScore(const ComputeNode& node)
{
    // Calculate overall score for node selection
    int available_capacity = node.capacity - node.current_load;
    return available_capacity;
}

void LoadBalancer::updateStrategy()
{
    // Adaptive strategy selection based on current cluster state
    auto stats = getLoadStats();
    
    if (stats.find("load_variance") != stats.end()) {
        double variance = stats["load_variance"];
        
        // If high variance, use least loaded to balance
        if (variance > 30.0) {
            if (strategy_ != LoadBalancingStrategy::LEAST_LOADED) {
                logger_.info("Adaptive mode: switching to LEAST_LOADED due to high variance");
                strategy_ = LoadBalancingStrategy::LEAST_LOADED;
            }
        }
    }
}
