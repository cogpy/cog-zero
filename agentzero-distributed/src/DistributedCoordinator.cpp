/**
 * DistributedCoordinator.cpp
 * 
 * Implementation of DistributedCoordinator for Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 */

#include <opencog/agentzero/distributed/DistributedCoordinator.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <sstream>
#include <random>
#include <algorithm>

using namespace opencog;
using namespace opencog::agentzero;

DistributedCoordinator::DistributedCoordinator(AtomSpacePtr atomspace, 
                                               const std::string& coordinator_id)
    : atomspace_(atomspace),
      coordinator_id_(coordinator_id),
      logger_("DistributedCoordinator")
{
    logger_.info("DistributedCoordinator created with ID: %s", coordinator_id.c_str());
}

DistributedCoordinator::~DistributedCoordinator()
{
    shutdown();
}

bool DistributedCoordinator::registerNode(const std::string& node_id,
                                         const std::string& hostname,
                                         int port)
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    // Check if node already registered
    if (nodes_.find(node_id) != nodes_.end()) {
        logger_.warn("Node %s already registered", node_id.c_str());
        return false;
    }
    
    // Create and register node
    ComputeNode node(node_id, hostname, port);
    nodes_[node_id] = node;
    
    logger_.info("Registered node: %s at %s:%d", 
                node_id.c_str(), hostname.c_str(), port);
    
    // Store node information in AtomSpace
    Handle node_atom = atomspace_->add_node(CONCEPT_NODE, "ComputeNode:" + node_id);
    Handle hostname_atom = atomspace_->add_node(CONCEPT_NODE, hostname);
    atomspace_->add_link(EVALUATION_LINK, {node_atom, hostname_atom});
    
    return true;
}

bool DistributedCoordinator::unregisterNode(const std::string& node_id)
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        logger_.warn("Node %s not found for unregistration", node_id.c_str());
        return false;
    }
    
    // Mark node as inactive
    it->second.is_active = false;
    nodes_.erase(it);
    
    logger_.info("Unregistered node: %s", node_id.c_str());
    
    // Remove from AtomSpace
    Handle node_atom = atomspace_->get_node(CONCEPT_NODE, "ComputeNode:" + node_id);
    if (node_atom != Handle::UNDEFINED) {
        atomspace_->remove_atom(node_atom, true);
    }
    
    return true;
}

std::string DistributedCoordinator::submitTask(const std::string& task_type,
                                              Handle task_atom)
{
    std::string task_id = generateTaskId();
    
    // Select optimal node for this task
    std::string selected_node = selectNodeForTask(task_type);
    
    if (selected_node.empty()) {
        logger_.error("No available node for task type: %s", task_type.c_str());
        return "";
    }
    
    // Create task
    DistributedTask task(task_id, task_type, task_atom);
    task.assigned_node = selected_node;
    
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        tasks_[task_id] = task;
    }
    
    // Update node load
    updateNodeLoad(selected_node, 1);
    
    logger_.info("Submitted task %s (type: %s) to node %s",
                task_id.c_str(), task_type.c_str(), selected_node.c_str());
    
    // Store task in AtomSpace
    Handle task_node = atomspace_->add_node(CONCEPT_NODE, "Task:" + task_id);
    Handle type_node = atomspace_->add_node(CONCEPT_NODE, task_type);
    atomspace_->add_link(INHERITANCE_LINK, {task_node, type_node});
    
    return task_id;
}

bool DistributedCoordinator::isTaskCompleted(const std::string& task_id) const
{
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return false;
    }
    
    return it->second.completed;
}

Handle DistributedCoordinator::getTaskResult(const std::string& task_id) const
{
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto it = tasks_.find(task_id);
    if (it == tasks_.end() || !it->second.completed) {
        return Handle::UNDEFINED;
    }
    
    // Look up result in AtomSpace
    Handle result_node = atomspace_->get_node(CONCEPT_NODE, "TaskResult:" + task_id);
    return result_node;
}

std::vector<ComputeNode> DistributedCoordinator::getRegisteredNodes() const
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::vector<ComputeNode> result;
    result.reserve(nodes_.size());
    
    for (const auto& pair : nodes_) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::map<std::string, int> DistributedCoordinator::getClusterStats() const
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::map<std::string, int> stats;
    
    int total_nodes = nodes_.size();
    int active_nodes = 0;
    int total_capacity = 0;
    int total_load = 0;
    
    for (const auto& pair : nodes_) {
        if (pair.second.is_active) {
            active_nodes++;
        }
        total_capacity += pair.second.capacity;
        total_load += pair.second.current_load;
    }
    
    stats["total_nodes"] = total_nodes;
    stats["active_nodes"] = active_nodes;
    stats["total_capacity"] = total_capacity;
    stats["total_load"] = total_load;
    stats["available_capacity"] = total_capacity - total_load;
    
    return stats;
}

void DistributedCoordinator::healthCheck()
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    logger_.info("Performing health check on %zu nodes", nodes_.size());
    
    std::vector<std::string> inactive_nodes;
    
    for (auto& pair : nodes_) {
        // Simple health check - in real implementation, would ping nodes
        // For now, just check if node is marked active
        if (!pair.second.is_active) {
            inactive_nodes.push_back(pair.first);
        }
    }
    
    logger_.info("Health check complete. %zu inactive nodes found", 
                inactive_nodes.size());
}

void DistributedCoordinator::setTaskCompletionCallback(
    std::function<void(const std::string&)> callback)
{
    task_completion_callback_ = callback;
}

void DistributedCoordinator::shutdown()
{
    logger_.info("Shutting down DistributedCoordinator");
    
    {
        std::lock_guard<std::mutex> lock(nodes_mutex_);
        for (auto& pair : nodes_) {
            pair.second.is_active = false;
        }
        nodes_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        tasks_.clear();
    }
}

// Private helper methods

std::string DistributedCoordinator::selectNodeForTask(const std::string& task_type)
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    if (nodes_.empty()) {
        return "";
    }
    
    // Simple least-loaded selection
    std::string selected_node;
    int min_load = INT_MAX;
    
    for (const auto& pair : nodes_) {
        if (pair.second.is_active && pair.second.current_load < min_load) {
            min_load = pair.second.current_load;
            selected_node = pair.first;
        }
    }
    
    return selected_node;
}

void DistributedCoordinator::updateNodeLoad(const std::string& node_id, int delta)
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        it->second.current_load += delta;
        
        // Ensure load stays within bounds
        if (it->second.current_load < 0) {
            it->second.current_load = 0;
        }
    }
}

std::string DistributedCoordinator::generateTaskId()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    std::ostringstream oss;
    oss << coordinator_id_ << "-task-" << dis(gen);
    return oss.str();
}
