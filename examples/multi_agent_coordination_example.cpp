/*
 * Multi-Agent Coordination Example
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Demonstrates multi-agent coordination protocols including:
 * - Agent registration and discovery
 * - Task delegation with capability matching
 * - Consensus-based decision making
 * - Resource allocation
 * - Conflict detection and resolution
 *
 * Part of the AGENT-ZERO-GENESIS project - AZ-MULTI-001
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/communication/MultiAgentCoordinator.h"
#include "opencog/agentzero/communication/AgentComms.h"

using namespace opencog;
using namespace opencog::agentzero::communication;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void printAgentInfo(const AgentRegistration& agent) {
    std::cout << "Agent: " << agent.agent_id.toString() << std::endl;
    std::cout << "  Status: " << agent.status << std::endl;
    std::cout << "  Load: " << (agent.load_factor * 100.0) << "%" << std::endl;
    std::cout << "  Capabilities:" << std::endl;
    for (const auto& cap : agent.capabilities) {
        std::cout << "    - " << cap.capability_name 
                  << " (proficiency: " << (cap.proficiency * 100.0) << "%)" << std::endl;
    }
}

void printTaskInfo(const CoordinationTask& task) {
    std::cout << "Task: " << task.task_id << std::endl;
    std::cout << "  Description: " << task.description << std::endl;
    std::cout << "  Status: " << task.status << std::endl;
    if (!task.assigned_to.name.empty()) {
        std::cout << "  Assigned to: " << task.assigned_to.toString() << std::endl;
    }
    std::cout << "  Required capabilities:" << std::endl;
    for (const auto& cap : task.required_capabilities) {
        std::cout << "    - " << cap << std::endl;
    }
}

void printStatistics(const CoordinationStats& stats) {
    std::cout << "Coordination Statistics:" << std::endl;
    std::cout << "  Total Agents: " << stats.total_agents << std::endl;
    std::cout << "  Active Agents: " << stats.active_agents << std::endl;
    std::cout << "  Total Tasks: " << stats.total_tasks << std::endl;
    std::cout << "  Completed Tasks: " << stats.completed_tasks << std::endl;
    std::cout << "  Total Proposals: " << stats.total_proposals << std::endl;
    std::cout << "  Accepted Proposals: " << stats.accepted_proposals << std::endl;
    std::cout << "  Total Conflicts: " << stats.total_conflicts << std::endl;
    std::cout << "  Resolved Conflicts: " << stats.resolved_conflicts << std::endl;
}

int main() {
    // Configure logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "Multi-Agent Coordination Protocols Demo" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Create AtomSpace
    AtomSpacePtr atomspace = createAtomSpace();
    
    // Create communication system
    AgentId coordinator_id("MainCoordinator", "instance1");
    CommConfig config;
    config.enable_persistence = true;
    auto comms = std::make_shared<AgentComms>(coordinator_id, config);
    
    // Create multi-agent coordinator
    auto coordinator = std::make_unique<MultiAgentCoordinator>(
        atomspace, comms, coordinator_id);
    
    if (!coordinator->initialize()) {
        std::cerr << "Failed to initialize coordinator" << std::endl;
        return 1;
    }
    
    std::cout << "✓ Coordinator initialized successfully" << std::endl;
    
    // ==============================================================
    // 1. Agent Registration
    // ==============================================================
    
    printSeparator("1. Agent Registration and Discovery");
    
    // Register multiple agents with different capabilities
    AgentId planner("PlannerAgent", "inst1");
    std::vector<AgentCapability> planner_caps = {
        AgentCapability("planning", 0.95),
        AgentCapability("reasoning", 0.85)
    };
    coordinator->registerAgent(planner, planner_caps);
    std::cout << "✓ Registered PlannerAgent" << std::endl;
    
    AgentId executor("ExecutorAgent", "inst1");
    std::vector<AgentCapability> executor_caps = {
        AgentCapability("execution", 0.90),
        AgentCapability("monitoring", 0.80)
    };
    coordinator->registerAgent(executor, executor_caps);
    std::cout << "✓ Registered ExecutorAgent" << std::endl;
    
    AgentId learner("LearnerAgent", "inst1");
    std::vector<AgentCapability> learner_caps = {
        AgentCapability("learning", 0.92),
        AgentCapability("reasoning", 0.88),
        AgentCapability("planning", 0.75)
    };
    coordinator->registerAgent(learner, learner_caps);
    std::cout << "✓ Registered LearnerAgent" << std::endl;
    
    std::cout << "\nRegistered Agents:" << std::endl;
    auto all_agents = coordinator->getRegisteredAgents();
    for (const auto& agent_id : all_agents) {
        auto info = coordinator->getAgentInfo(agent_id);
        if (info) {
            printAgentInfo(*info);
            std::cout << std::endl;
        }
    }
    
    // Find agents by capability
    std::cout << "\nAgents with 'planning' capability:" << std::endl;
    auto planning_agents = coordinator->findAgentsByCapability("planning");
    for (const auto& agent : planning_agents) {
        std::cout << "  - " << agent.toString() << std::endl;
    }
    
    std::cout << "\nAgents with 'reasoning' capability:" << std::endl;
    auto reasoning_agents = coordinator->findAgentsByCapability("reasoning");
    for (const auto& agent : reasoning_agents) {
        std::cout << "  - " << agent.toString() << std::endl;
    }
    
    // ==============================================================
    // 2. Task Delegation
    // ==============================================================
    
    printSeparator("2. Task Delegation and Distribution");
    
    // Create tasks with different requirements
    std::string task1 = coordinator->createTask(
        "Analyze system logs and identify patterns",
        {"planning", "reasoning"},
        MessagePriority::HIGH
    );
    std::cout << "✓ Created Task 1: " << task1 << std::endl;
    
    std::string task2 = coordinator->createTask(
        "Execute maintenance routine",
        {"execution", "monitoring"},
        MessagePriority::NORMAL
    );
    std::cout << "✓ Created Task 2: " << task2 << std::endl;
    
    std::string task3 = coordinator->createTask(
        "Learn from recent interactions",
        {"learning", "reasoning"},
        MessagePriority::LOW
    );
    std::cout << "✓ Created Task 3: " << task3 << std::endl;
    
    // Auto-assign tasks based on capabilities
    std::cout << "\nAuto-assigning tasks..." << std::endl;
    coordinator->assignTask(task1);
    coordinator->assignTask(task2);
    coordinator->assignTask(task3);
    
    std::cout << "\nTask Assignments:" << std::endl;
    auto pending_tasks = coordinator->getTasksByStatus("assigned");
    for (const auto& task_id : pending_tasks) {
        auto task = coordinator->getTaskInfo(task_id);
        if (task) {
            printTaskInfo(*task);
            std::cout << std::endl;
        }
    }
    
    // Simulate task progress
    std::cout << "Simulating task execution..." << std::endl;
    coordinator->updateTaskStatus(task1, "in_progress");
    coordinator->updateTaskStatus(task2, "in_progress");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    coordinator->completeTask(task1);
    coordinator->completeTask(task2);
    std::cout << "✓ Tasks 1 and 2 completed" << std::endl;
    
    // ==============================================================
    // 3. Consensus Mechanism
    // ==============================================================
    
    printSeparator("3. Consensus-Based Decision Making");
    
    std::string proposal1 = coordinator->createProposal(
        "Adopt new learning algorithm",
        0.6,  // 60% agreement required
        std::chrono::seconds(60)
    );
    std::cout << "✓ Created Proposal: " << proposal1 << std::endl;
    
    // Agents vote on the proposal
    std::cout << "\nVoting on proposal..." << std::endl;
    coordinator->castVote(proposal1, planner, true);
    std::cout << "  PlannerAgent: YES" << std::endl;
    
    coordinator->castVote(proposal1, executor, false);
    std::cout << "  ExecutorAgent: NO" << std::endl;
    
    coordinator->castVote(proposal1, learner, true);
    std::cout << "  LearnerAgent: YES" << std::endl;
    
    auto proposal = coordinator->getProposalInfo(proposal1);
    if (proposal) {
        size_t yes_votes = 0;
        size_t total_votes = proposal->votes.size();
        for (const auto& vote : proposal->votes) {
            if (vote.second) yes_votes++;
        }
        
        double approval = static_cast<double>(yes_votes) / total_votes;
        std::cout << "\nVoting Results:" << std::endl;
        std::cout << "  YES: " << yes_votes << " / " << total_votes << std::endl;
        std::cout << "  Approval: " << (approval * 100.0) << "%" << std::endl;
        std::cout << "  Threshold: " << (proposal->threshold * 100.0) << "%" << std::endl;
        
        if (approval >= proposal->threshold) {
            std::cout << "  Status: ACCEPTED ✓" << std::endl;
        } else {
            std::cout << "  Status: REJECTED ✗" << std::endl;
        }
    }
    
    // ==============================================================
    // 4. Resource Allocation
    // ==============================================================
    
    printSeparator("4. Resource Allocation Management");
    
    // Register available resources
    coordinator->registerResource("CPU", 100.0);
    coordinator->registerResource("Memory", 1000.0);
    coordinator->registerResource("GPU", 10.0);
    std::cout << "✓ Registered resources" << std::endl;
    
    std::cout << "\nInitial Resource Availability:" << std::endl;
    std::cout << "  CPU: " << coordinator->getAvailableResource("CPU") << " units" << std::endl;
    std::cout << "  Memory: " << coordinator->getAvailableResource("Memory") << " MB" << std::endl;
    std::cout << "  GPU: " << coordinator->getAvailableResource("GPU") << " units" << std::endl;
    
    // Agents request resources
    std::cout << "\nAllocating resources..." << std::endl;
    std::string req1 = coordinator->requestResource(planner, "CPU", 30.0);
    std::cout << "  PlannerAgent: CPU = 30 units" << std::endl;
    
    std::string req2 = coordinator->requestResource(executor, "Memory", 400.0);
    std::cout << "  ExecutorAgent: Memory = 400 MB" << std::endl;
    
    std::string req3 = coordinator->requestResource(learner, "GPU", 5.0);
    std::cout << "  LearnerAgent: GPU = 5 units" << std::endl;
    
    std::cout << "\nRemaining Resource Availability:" << std::endl;
    std::cout << "  CPU: " << coordinator->getAvailableResource("CPU") << " units" << std::endl;
    std::cout << "  Memory: " << coordinator->getAvailableResource("Memory") << " MB" << std::endl;
    std::cout << "  GPU: " << coordinator->getAvailableResource("GPU") << " units" << std::endl;
    
    // Release some resources
    std::cout << "\nReleasing resources..." << std::endl;
    coordinator->releaseResource(req1);
    std::cout << "  Released CPU allocation" << std::endl;
    
    std::cout << "\nFinal Resource Availability:" << std::endl;
    std::cout << "  CPU: " << coordinator->getAvailableResource("CPU") << " units" << std::endl;
    std::cout << "  Memory: " << coordinator->getAvailableResource("Memory") << " MB" << std::endl;
    std::cout << "  GPU: " << coordinator->getAvailableResource("GPU") << " units" << std::endl;
    
    // ==============================================================
    // 5. Conflict Detection
    // ==============================================================
    
    printSeparator("5. Conflict Detection and Resolution");
    
    // Create a conflict scenario by overloading an agent
    std::cout << "Creating conflict scenario..." << std::endl;
    for (int i = 0; i < 8; i++) {
        std::string task_id = coordinator->createTask(
            "Overload task " + std::to_string(i),
            {"execution"}
        );
        coordinator->assignTask(task_id, executor);
    }
    std::cout << "✓ Assigned 8 tasks to ExecutorAgent" << std::endl;
    
    // Detect conflicts
    auto conflicts = coordinator->detectConflicts();
    std::cout << "\nDetected Conflicts:" << std::endl;
    if (conflicts.empty()) {
        std::cout << "  No conflicts detected" << std::endl;
    } else {
        for (const auto& conflict : conflicts) {
            std::cout << "  ⚠ " << conflict << std::endl;
        }
    }
    
    // Resolve conflicts
    if (!conflicts.empty()) {
        std::cout << "\nResolving conflicts..." << std::endl;
        coordinator->resolveConflict("overload", "redistribute_tasks");
        std::cout << "✓ Conflict resolution initiated" << std::endl;
    }
    
    // ==============================================================
    // 6. State Synchronization
    // ==============================================================
    
    printSeparator("6. State Synchronization");
    
    std::cout << "Broadcasting state update..." << std::endl;
    size_t notified = coordinator->broadcastStateUpdate(
        "system_config",
        "New coordination parameters applied"
    );
    std::cout << "✓ Notified " << notified << " agents" << std::endl;
    
    // ==============================================================
    // 7. Final Statistics
    // ==============================================================
    
    printSeparator("7. Final Statistics and System Health");
    
    auto stats = coordinator->getStatistics();
    printStatistics(stats);
    
    std::cout << "\nSystem Health Check:" << std::endl;
    std::cout << "  Active Agents: " << coordinator->getActiveAgentsCount() << std::endl;
    std::cout << "  Pending Tasks: " << coordinator->getPendingTasksCount() << std::endl;
    std::cout << "  System Status: " << (coordinator->isHealthy() ? "HEALTHY ✓" : "UNHEALTHY ✗") << std::endl;
    
    // ==============================================================
    // Cleanup
    // ==============================================================
    
    printSeparator("Cleanup");
    
    coordinator->shutdown();
    std::cout << "✓ Coordinator shutdown complete" << std::endl;
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Multi-Agent Coordination Demo Complete!" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    return 0;
}
