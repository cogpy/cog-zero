/*
 * opencog/agentzero/tools/CapabilityComposer.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * CapabilityComposer Implementation
 * Combines tools and capabilities for complex task execution
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 * Task ID: AZ-TOOL-003
 */

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <random>
#include <chrono>
#include <queue>
#include <stack>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include "opencog/agentzero/tools/CapabilityComposer.h"

namespace opencog {
namespace agentzero {
namespace tools {

CapabilityComposer::CapabilityComposer(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _capability_registry_base(Handle::UNDEFINED)
    , _composition_plans_node(Handle::UNDEFINED)
    , _execution_history_node(Handle::UNDEFINED)
    , _max_cached_plans(100)
    , _composition_timeout_seconds(30.0)
    , _enable_automatic_composition(true)
    , _enable_parallel_execution(false)
    , _max_composition_depth(10)
{
    if (!_atomspace) {
        throw std::invalid_argument("CapabilityComposer requires valid AtomSpace");
    }
    
    initializeCapabilityStructures();
    logger().info("CapabilityComposer: Initialized capability composition system");
}

CapabilityComposer::~CapabilityComposer()
{
    logger().info("CapabilityComposer: Destroyed with %zu capabilities and %zu plans", 
                  _capabilities.size(), _plans.size());
}

void CapabilityComposer::initializeCapabilityStructures()
{
    // Create base structure in AtomSpace
    _capability_registry_base = _atomspace->add_node(CONCEPT_NODE, "CapabilityRegistry");
    _composition_plans_node = _atomspace->add_node(CONCEPT_NODE, "CompositionPlans");
    _execution_history_node = _atomspace->add_node(CONCEPT_NODE, "ExecutionHistory");
    
    // Create structural links
    _atomspace->add_link(INHERITANCE_LINK, {_composition_plans_node, _capability_registry_base});
    _atomspace->add_link(INHERITANCE_LINK, {_execution_history_node, _capability_registry_base});
    
    logger().debug("CapabilityComposer: Initialized AtomSpace structures");
}

bool CapabilityComposer::registerCapability(const Capability& capability)
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    
    try {
        // Check if already registered
        if (_capabilities.find(capability.capability_id) != _capabilities.end()) {
            logger().warn("CapabilityComposer: Capability %s already registered", 
                         capability.capability_id.c_str());
            return false;
        }
        
        // Validate capability
        if (capability.capability_id.empty() || capability.name.empty()) {
            logger().error("CapabilityComposer: Invalid capability - ID or name empty");
            return false;
        }
        
        // Store capability
        Capability cap_copy = capability;
        cap_copy.capability_atom = createCapabilityAtom(capability);
        _capabilities[capability.capability_id] = cap_copy;
        
        // Index what this capability provides
        for (const auto& provided : capability.provided_capabilities) {
            _capability_providers[provided].insert(capability.capability_id);
        }
        
        // Store dependencies
        _capability_dependencies[capability.capability_id] = capability.required_capabilities;
        
        logger().info("CapabilityComposer: Registered capability '%s' (%s)", 
                     capability.name.c_str(), capability.capability_id.c_str());
        
        return true;
        
    } catch (const std::exception& e) {
        logger().error("CapabilityComposer: Error registering capability: %s", e.what());
        return false;
    }
}

bool CapabilityComposer::registerCapability(const std::string& capability_id,
                                          const std::string& name,
                                          const std::string& description,
                                          CapabilityFunction execute,
                                          const std::vector<std::string>& required_capabilities)
{
    Capability capability;
    capability.capability_id = capability_id;
    capability.name = name;
    capability.description = description;
    capability.execute = execute;
    capability.required_capabilities = required_capabilities;
    capability.is_atomic = true;
    
    // Set default provided capability (same as ID)
    capability.provided_capabilities.push_back(capability_id);
    
    return registerCapability(capability);
}

bool CapabilityComposer::unregisterCapability(const std::string& capability_id)
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    
    auto it = _capabilities.find(capability_id);
    if (it == _capabilities.end()) {
        return false;
    }
    
    // Remove from provider index
    for (const auto& provided : it->second.provided_capabilities) {
        _capability_providers[provided].erase(capability_id);
    }
    
    // Remove from dependencies
    _capability_dependencies.erase(capability_id);
    
    // Remove capability
    _capabilities.erase(it);
    
    logger().info("CapabilityComposer: Unregistered capability %s", capability_id.c_str());
    return true;
}

const CapabilityComposer::Capability* CapabilityComposer::getCapability(const std::string& capability_id) const
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    
    auto it = _capabilities.find(capability_id);
    if (it != _capabilities.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<std::string> CapabilityComposer::findCapabilitiesProviding(const std::string& output_type)
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    
    std::vector<std::string> result;
    auto it = _capability_providers.find(output_type);
    
    if (it != _capability_providers.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    
    return result;
}

Handle CapabilityComposer::createCapabilityAtom(const Capability& capability)
{
    // Create capability node
    Handle capability_node = _atomspace->add_node(CONCEPT_NODE, 
        "Capability_" + capability.capability_id);
    
    // Create description link
    Handle desc_predicate = _atomspace->add_node(PREDICATE_NODE, "has_description");
    Handle desc_node = _atomspace->add_node(CONCEPT_NODE, capability.description);
    _atomspace->add_link(EVALUATION_LINK, {
        desc_predicate,
        _atomspace->add_link(LIST_LINK, {capability_node, desc_node})
    });
    
    // Link required capabilities (dependencies)
    for (const auto& required : capability.required_capabilities) {
        Handle req_capability = _atomspace->add_node(CONCEPT_NODE, "Capability_" + required);
        Handle requires_pred = _atomspace->add_node(PREDICATE_NODE, "requires_capability");
        _atomspace->add_link(EVALUATION_LINK, {
            requires_pred,
            _atomspace->add_link(LIST_LINK, {capability_node, req_capability})
        });
    }
    
    // Link provided capabilities
    for (const auto& provided : capability.provided_capabilities) {
        Handle prov_node = _atomspace->add_node(CONCEPT_NODE, "Output_" + provided);
        Handle provides_pred = _atomspace->add_node(PREDICATE_NODE, "provides_output");
        _atomspace->add_link(EVALUATION_LINK, {
            provides_pred,
            _atomspace->add_link(LIST_LINK, {capability_node, prov_node})
        });
    }
    
    // Link to capability registry
    _atomspace->add_link(MEMBER_LINK, {capability_node, _capability_registry_base});
    
    return capability_node;
}

Handle CapabilityComposer::createCompositionPlanAtom(const CompositionPlan& plan)
{
    // Create plan node
    Handle plan_node = _atomspace->add_node(CONCEPT_NODE, "Plan_" + plan.plan_id);
    
    // Create sequence link
    HandleSeq sequence_handles;
    for (const auto& cap_id : plan.capability_sequence) {
        sequence_handles.push_back(_atomspace->add_node(CONCEPT_NODE, "Capability_" + cap_id));
    }
    
    if (!sequence_handles.empty()) {
        Handle sequence_link = _atomspace->add_link(ORDERED_LINK, sequence_handles);
        Handle sequence_pred = _atomspace->add_node(PREDICATE_NODE, "execution_sequence");
        _atomspace->add_link(EVALUATION_LINK, {
            sequence_pred,
            _atomspace->add_link(LIST_LINK, {plan_node, sequence_link})
        });
    }
    
    // Link to composition plans
    _atomspace->add_link(MEMBER_LINK, {plan_node, _composition_plans_node});
    
    return plan_node;
}

CapabilityComposer::CompositionPlan CapabilityComposer::composeForTask(
    const TaskRequirements& requirements)
{
    std::lock_guard<std::mutex> lock(_plans_mutex);
    
    CompositionPlan plan;
    
    try {
        // Check if we already have a plan for this task
        auto it = _task_to_plans.find(requirements.task_description);
        if (it != _task_to_plans.end() && !it->second.empty()) {
            // Return cached plan
            const std::string& plan_id = it->second.front();
            auto plan_it = _plans.find(plan_id);
            if (plan_it != _plans.end()) {
                logger().debug("CapabilityComposer: Using cached plan for task");
                return plan_it->second;
            }
        }
        
        // Plan new composition
        plan = planComposition(requirements);
        
        if (plan.is_valid) {
            // Create AtomSpace representation
            plan.plan_atom = createCompositionPlanAtom(plan);
            
            // Cache the plan
            _plans[plan.plan_id] = plan;
            _task_to_plans[requirements.task_description].push_back(plan.plan_id);
            
            // Enforce plan limit
            while (_plans.size() > _max_cached_plans) {
                auto oldest = _plans.begin();
                _plans.erase(oldest);
            }
            
            logger().info("CapabilityComposer: Created plan %s with %zu steps", 
                         plan.plan_id.c_str(), plan.capability_sequence.size());
        } else {
            logger().warn("CapabilityComposer: Failed to create valid plan for task");
        }
        
    } catch (const std::exception& e) {
        logger().error("CapabilityComposer: Error composing plan: %s", e.what());
        plan.is_valid = false;
    }
    
    return plan;
}

CapabilityComposer::CompositionPlan CapabilityComposer::planComposition(
    const TaskRequirements& requirements)
{
    CompositionPlan plan;
    plan.plan_id = generatePlanId();
    plan.goal_description = requirements.task_description;
    plan.is_valid = false;
    
    // Find capabilities that can provide required outputs
    std::vector<std::string> candidate_capabilities;
    
    for (const auto& required_output : requirements.required_outputs) {
        auto providers = findCapabilitiesProviding(required_output);
        candidate_capabilities.insert(candidate_capabilities.end(), 
                                     providers.begin(), providers.end());
    }
    
    if (candidate_capabilities.empty()) {
        logger().warn("CapabilityComposer: No capabilities found for required outputs");
        return plan;
    }
    
    // Resolve dependencies and create execution sequence
    plan.capability_sequence = resolveDependencies(candidate_capabilities);
    
    if (plan.capability_sequence.empty()) {
        logger().warn("CapabilityComposer: Failed to resolve capability dependencies");
        return plan;
    }
    
    // Build dependency graph
    for (const auto& cap_id : plan.capability_sequence) {
        auto it = _capability_dependencies.find(cap_id);
        if (it != _capability_dependencies.end()) {
            plan.dependency_graph[cap_id] = it->second;
        }
    }
    
    // Validate the plan
    plan.is_valid = validateComposition(plan);
    
    if (plan.is_valid) {
        // Estimate success and execution time
        plan.estimated_success_probability = estimatePlanSuccess(plan);
        plan.estimated_execution_time = estimatePlanExecutionTime(plan);
        
        // Find parallel execution opportunities if enabled
        if (_enable_parallel_execution) {
            auto parallel_groups = findParallelGroups(plan);
            // Store parallel group information (simplified)
            for (const auto& group : parallel_groups) {
                for (const auto& cap_id : group) {
                    plan.parallel_groups.push_back(cap_id);
                }
            }
        }
    }
    
    return plan;
}

std::vector<std::string> CapabilityComposer::resolveDependencies(
    const std::vector<std::string>& capabilities)
{
    // Build complete dependency set
    std::set<std::string> all_capabilities;
    std::map<std::string, std::vector<std::string>> dep_graph;
    
    std::queue<std::string> to_process;
    for (const auto& cap_id : capabilities) {
        to_process.push(cap_id);
    }
    
    // BFS to collect all dependencies
    size_t depth = 0;
    while (!to_process.empty() && depth < _max_composition_depth) {
        size_t level_size = to_process.size();
        
        for (size_t i = 0; i < level_size; ++i) {
            std::string cap_id = to_process.front();
            to_process.pop();
            
            if (all_capabilities.find(cap_id) != all_capabilities.end()) {
                continue;  // Already processed
            }
            
            all_capabilities.insert(cap_id);
            
            auto it = _capability_dependencies.find(cap_id);
            if (it != _capability_dependencies.end()) {
                dep_graph[cap_id] = it->second;
                
                for (const auto& dep : it->second) {
                    to_process.push(dep);
                }
            }
        }
        
        depth++;
    }
    
    // Perform topological sort
    return topologicalSort(dep_graph);
}

std::vector<std::string> CapabilityComposer::topologicalSort(
    const std::map<std::string, std::vector<std::string>>& deps)
{
    std::vector<std::string> result;
    std::map<std::string, int> in_degree;
    std::map<std::string, std::vector<std::string>> graph;
    
    // Build graph and calculate in-degrees
    for (const auto& pair : deps) {
        const std::string& node = pair.first;
        
        if (in_degree.find(node) == in_degree.end()) {
            in_degree[node] = 0;
        }
        
        for (const auto& dep : pair.second) {
            graph[dep].push_back(node);
            in_degree[node]++;
        }
    }
    
    // Kahn's algorithm
    std::queue<std::string> zero_in_degree;
    for (const auto& pair : in_degree) {
        if (pair.second == 0) {
            zero_in_degree.push(pair.first);
        }
    }
    
    while (!zero_in_degree.empty()) {
        std::string node = zero_in_degree.front();
        zero_in_degree.pop();
        result.push_back(node);
        
        for (const auto& neighbor : graph[node]) {
            in_degree[neighbor]--;
            if (in_degree[neighbor] == 0) {
                zero_in_degree.push(neighbor);
            }
        }
    }
    
    return result;
}

bool CapabilityComposer::validateComposition(const CompositionPlan& plan)
{
    if (plan.capability_sequence.empty()) {
        return false;
    }
    
    // Check all capabilities exist
    for (const auto& cap_id : plan.capability_sequence) {
        if (_capabilities.find(cap_id) == _capabilities.end()) {
            logger().warn("CapabilityComposer: Plan references unknown capability %s", 
                         cap_id.c_str());
            return false;
        }
    }
    
    // Check dependencies are satisfied by execution order
    std::set<std::string> available;
    
    for (const auto& cap_id : plan.capability_sequence) {
        // Check if dependencies are satisfied
        if (!checkDependencySatisfaction(cap_id, available)) {
            logger().warn("CapabilityComposer: Dependency not satisfied for %s", 
                         cap_id.c_str());
            return false;
        }
        
        // Mark outputs as available
        const auto* cap = getCapability(cap_id);
        if (cap) {
            for (const auto& provided : cap->provided_capabilities) {
                available.insert(provided);
            }
        }
    }
    
    return true;
}

bool CapabilityComposer::checkDependencySatisfaction(const std::string& capability_id,
                                                     const std::set<std::string>& available)
{
    auto it = _capability_dependencies.find(capability_id);
    if (it == _capability_dependencies.end()) {
        return true;  // No dependencies
    }
    
    for (const auto& required : it->second) {
        if (available.find(required) == available.end()) {
            return false;
        }
    }
    
    return true;
}

double CapabilityComposer::estimatePlanSuccess(const CompositionPlan& plan)
{
    if (plan.capability_sequence.empty()) {
        return 0.0;
    }
    
    double combined_probability = 1.0;
    
    for (const auto& cap_id : plan.capability_sequence) {
        const auto* cap = getCapability(cap_id);
        if (cap) {
            combined_probability *= cap->success_rate;
        }
    }
    
    return combined_probability;
}

double CapabilityComposer::estimatePlanExecutionTime(const CompositionPlan& plan)
{
    double total_time = 0.0;
    
    for (const auto& cap_id : plan.capability_sequence) {
        const auto* cap = getCapability(cap_id);
        if (cap) {
            total_time += cap->average_execution_time;
        }
    }
    
    return total_time;
}

std::vector<std::vector<std::string>> CapabilityComposer::findParallelGroups(
    const CompositionPlan& plan)
{
    std::vector<std::vector<std::string>> groups;
    
    // Simple heuristic: capabilities with no dependencies on each other can run in parallel
    // For now, return sequential execution (can be enhanced later)
    for (const auto& cap_id : plan.capability_sequence) {
        groups.push_back({cap_id});
    }
    
    return groups;
}

CapabilityComposer::CapabilityResult CapabilityComposer::executePlan(
    const CompositionPlan& plan, ExecutionContext& context)
{
    if (!plan.is_valid) {
        logger().error("CapabilityComposer: Attempted to execute invalid plan");
        return CapabilityResult::FAILURE;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    CapabilityResult overall_result = CapabilityResult::SUCCESS;
    
    logger().info("CapabilityComposer: Executing plan %s with %zu capabilities", 
                 plan.plan_id.c_str(), plan.capability_sequence.size());
    
    for (const auto& cap_id : plan.capability_sequence) {
        context.current_capability = cap_id;
        
        auto cap_start = std::chrono::steady_clock::now();
        CapabilityResult result = executeCapability(cap_id, context);
        auto cap_end = std::chrono::steady_clock::now();
        
        double execution_time = std::chrono::duration<double>(cap_end - cap_start).count();
        updateCapabilityStatistics(cap_id, result, execution_time);
        logExecution(cap_id, result, context);
        
        if (result == CapabilityResult::FAILURE) {
            logger().error("CapabilityComposer: Capability %s failed, aborting plan", 
                          cap_id.c_str());
            overall_result = CapabilityResult::FAILURE;
            break;
        } else if (result == CapabilityResult::PARTIAL_SUCCESS) {
            overall_result = CapabilityResult::PARTIAL_SUCCESS;
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    double total_time = std::chrono::duration<double>(end_time - start_time).count();
    
    logger().info("CapabilityComposer: Plan execution completed in %.2f seconds with result: %d", 
                 total_time, static_cast<int>(overall_result));
    
    return overall_result;
}

CapabilityComposer::CapabilityResult CapabilityComposer::executeCapability(
    const std::string& capability_id, ExecutionContext& context)
{
    const auto* capability = getCapability(capability_id);
    if (!capability) {
        logger().error("CapabilityComposer: Capability %s not found", capability_id.c_str());
        return CapabilityResult::FAILURE;
    }
    
    if (!capability->execute) {
        logger().error("CapabilityComposer: Capability %s has no execution function", 
                      capability_id.c_str());
        return CapabilityResult::FAILURE;
    }
    
    try {
        bool success = capability->execute(context);
        return success ? CapabilityResult::SUCCESS : CapabilityResult::FAILURE;
        
    } catch (const std::exception& e) {
        logger().error("CapabilityComposer: Exception executing capability %s: %s", 
                      capability_id.c_str(), e.what());
        return CapabilityResult::FAILURE;
    }
}

void CapabilityComposer::updateCapabilityStatistics(const std::string& capability_id,
                                                    CapabilityResult result,
                                                    double execution_time)
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    
    auto it = _capabilities.find(capability_id);
    if (it != _capabilities.end()) {
        Capability& cap = it->second;
        
        // Update execution count
        cap.execution_count++;
        
        // Update average execution time (running average)
        cap.average_execution_time = 
            (cap.average_execution_time * (cap.execution_count - 1) + execution_time) / 
            cap.execution_count;
        
        // Update success rate
        double success_value = (result == CapabilityResult::SUCCESS) ? 1.0 : 0.0;
        cap.success_rate = 
            (cap.success_rate * (cap.execution_count - 1) + success_value) / 
            cap.execution_count;
    }
}

void CapabilityComposer::logExecution(const std::string& capability_id,
                                     CapabilityResult result,
                                     const ExecutionContext& context)
{
    std::ostringstream log_entry;
    log_entry << "Capability: " << capability_id 
              << ", Result: " << static_cast<int>(result)
              << ", Context: " << context.context_id;
    
    logger().debug("CapabilityComposer: %s", log_entry.str().c_str());
}

CapabilityComposer::CapabilityResult CapabilityComposer::composeAndExecute(
    const TaskRequirements& requirements, ExecutionContext& context)
{
    // Compose plan
    CompositionPlan plan = composeForTask(requirements);
    
    if (!plan.is_valid) {
        logger().error("CapabilityComposer: Failed to compose valid plan for task");
        return CapabilityResult::FAILURE;
    }
    
    // Execute plan
    return executePlan(plan, context);
}

std::map<std::string, double> CapabilityComposer::getCapabilityStatistics() const
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    
    std::map<std::string, double> stats;
    stats["total_capabilities"] = static_cast<double>(_capabilities.size());
    stats["total_plans"] = static_cast<double>(_plans.size());
    
    if (!_capabilities.empty()) {
        double total_success_rate = 0.0;
        double total_exec_time = 0.0;
        int total_executions = 0;
        
        for (const auto& pair : _capabilities) {
            const Capability& cap = pair.second;
            total_success_rate += cap.success_rate;
            total_exec_time += cap.average_execution_time;
            total_executions += cap.execution_count;
        }
        
        stats["average_success_rate"] = total_success_rate / _capabilities.size();
        stats["average_execution_time"] = total_exec_time / _capabilities.size();
        stats["total_executions"] = static_cast<double>(total_executions);
    } else {
        stats["average_success_rate"] = 0.0;
        stats["average_execution_time"] = 0.0;
        stats["total_executions"] = 0.0;
    }
    
    return stats;
}

std::vector<std::string> CapabilityComposer::getRegisteredCapabilities() const
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    
    std::vector<std::string> result;
    for (const auto& pair : _capabilities) {
        result.push_back(pair.first);
    }
    
    return result;
}

bool CapabilityComposer::isCapabilityRegistered(const std::string& capability_id) const
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    return _capabilities.find(capability_id) != _capabilities.end();
}

bool CapabilityComposer::validateCapabilityDependencies(const std::string& capability_id) const
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    
    auto it = _capability_dependencies.find(capability_id);
    if (it == _capability_dependencies.end()) {
        return true;  // No dependencies
    }
    
    for (const auto& dep : it->second) {
        if (_capabilities.find(dep) == _capabilities.end()) {
            return false;  // Missing dependency
        }
    }
    
    return true;
}

std::map<std::string, std::vector<std::string>> CapabilityComposer::getCapabilityDependencyTree(
    const std::string& capability_id, size_t max_depth) const
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    
    std::map<std::string, std::vector<std::string>> tree;
    std::queue<std::pair<std::string, size_t>> to_process;
    std::set<std::string> visited;
    
    to_process.push({capability_id, 0});
    
    while (!to_process.empty()) {
        auto [current_id, depth] = to_process.front();
        to_process.pop();
        
        if (depth >= max_depth || visited.find(current_id) != visited.end()) {
            continue;
        }
        
        visited.insert(current_id);
        
        auto it = _capability_dependencies.find(current_id);
        if (it != _capability_dependencies.end()) {
            tree[current_id] = it->second;
            
            for (const auto& dep : it->second) {
                to_process.push({dep, depth + 1});
            }
        }
    }
    
    return tree;
}

std::string CapabilityComposer::exportCapabilities(const std::string& format) const
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    
    if (format == "json") {
        std::ostringstream oss;
        oss << "{\n  \"capabilities\": [\n";
        
        bool first = true;
        for (const auto& pair : _capabilities) {
            const Capability& cap = pair.second;
            
            if (!first) oss << ",\n";
            first = false;
            
            oss << "    {\n";
            oss << "      \"id\": \"" << cap.capability_id << "\",\n";
            oss << "      \"name\": \"" << cap.name << "\",\n";
            oss << "      \"description\": \"" << cap.description << "\",\n";
            oss << "      \"success_rate\": " << cap.success_rate << ",\n";
            oss << "      \"execution_count\": " << cap.execution_count << ",\n";
            oss << "      \"average_execution_time\": " << cap.average_execution_time << "\n";
            oss << "    }";
        }
        
        oss << "\n  ]\n}";
        return oss.str();
    }
    
    return "Unsupported export format";
}

size_t CapabilityComposer::clearCachedPlans()
{
    std::lock_guard<std::mutex> lock(_plans_mutex);
    
    size_t count = _plans.size();
    _plans.clear();
    _task_to_plans.clear();
    
    logger().info("CapabilityComposer: Cleared %zu cached plans", count);
    return count;
}

size_t CapabilityComposer::getCapabilityCount() const
{
    std::lock_guard<std::mutex> lock(_capabilities_mutex);
    return _capabilities.size();
}

size_t CapabilityComposer::getPlanCount() const
{
    std::lock_guard<std::mutex> lock(_plans_mutex);
    return _plans.size();
}

std::string CapabilityComposer::generatePlanId() const
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    return "plan_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

std::string CapabilityComposer::generateContextId() const
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100, 999);
    
    return "ctx_" + std::to_string(dis(gen));
}

} // namespace tools
} // namespace agentzero
} // namespace opencog
