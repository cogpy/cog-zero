/*
 * tests/ActionSchedulerTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for ActionScheduler temporal coordination
 */

#include <cxxtest/TestSuite.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/ActionScheduler.h"
#include "opencog/agentzero/AgentZeroCore.h"

using namespace opencog;
using namespace opencog::agentzero;

class ActionSchedulerTest : public CxxTest::TestSuite
{
private:
    AtomSpacePtr atomspace;
    AgentZeroCore* agent_core;
    ActionScheduler* scheduler;
    
public:
    void setUp() override
    {
        // Create AtomSpace for testing
        atomspace = std::make_shared<AtomSpace>();
        
        // Create a minimal AgentZeroCore for testing
        agent_core = new AgentZeroCore();
        agent_core->initialize("TestAgent", atomspace);
        
        // Create ActionScheduler
        scheduler = new ActionScheduler(agent_core, atomspace);
    }
    
    void tearDown() override
    {
        delete scheduler;
        delete agent_core;
        atomspace.reset();
    }
    
    void test_constructor()
    {
        TS_ASSERT(scheduler != nullptr);
        TS_ASSERT(scheduler->isEnabled());
        TS_ASSERT_EQUALS(scheduler->getPendingActionCount(), 0);
        TS_ASSERT_EQUALS(scheduler->getExecutingActionCount(), 0);
    }
    
    void test_scheduler_contexts()
    {
        Handle scheduler_context = scheduler->getSchedulerContext();
        Handle active_actions_context = scheduler->getActiveActionsContext();
        Handle temporal_context = scheduler->getTemporalContext();
        
        TS_ASSERT(scheduler_context != Handle::UNDEFINED);
        TS_ASSERT(active_actions_context != Handle::UNDEFINED);
        TS_ASSERT(temporal_context != Handle::UNDEFINED);
        
        // Verify contexts exist in AtomSpace
        TS_ASSERT(atomspace->get_handle(scheduler_context));
        TS_ASSERT(atomspace->get_handle(active_actions_context));
        TS_ASSERT(atomspace->get_handle(temporal_context));
    }
    
    void test_immediate_action_scheduling()
    {
        // Create test action
        Handle test_action = atomspace->add_node(CONCEPT_NODE, "TestAction1");
        
        // Schedule immediate action
        bool scheduled = scheduler->scheduleImmediateAction(
            test_action,
            std::chrono::milliseconds(50),
            1  // priority
        );
        
        TS_ASSERT(scheduled);
        TS_ASSERT_EQUALS(scheduler->getPendingActionCount(), 1);
    }
    
    void test_timed_action_scheduling()
    {
        // Create test action
        Handle test_action = atomspace->add_node(CONCEPT_NODE, "TestAction2");
        
        // Schedule action for future execution
        auto future_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        bool scheduled = scheduler->scheduleAction(
            test_action,
            future_time,
            std::chrono::milliseconds(25),
            2  // priority
        );
        
        TS_ASSERT(scheduled);
        TS_ASSERT_EQUALS(scheduler->getPendingActionCount(), 1);
    }
    
    void test_action_processing()
    {
        // Create multiple test actions
        Handle action1 = atomspace->add_node(CONCEPT_NODE, "ProcessTestAction1");
        Handle action2 = atomspace->add_node(CONCEPT_NODE, "ProcessTestAction2");
        
        // Schedule immediate actions with different priorities
        scheduler->scheduleImmediateAction(action1, std::chrono::milliseconds(10), 1);
        scheduler->scheduleImmediateAction(action2, std::chrono::milliseconds(10), 2);
        
        TS_ASSERT_EQUALS(scheduler->getPendingActionCount(), 2);
        
        // Process scheduled actions
        bool processed = scheduler->processScheduledActions();
        TS_ASSERT(processed);
        
        // Actions should be completed (in this simple implementation)
        TS_ASSERT_EQUALS(scheduler->getPendingActionCount(), 0);
    }
    
    void test_scheduler_configuration()
    {
        // Test temporal parameter configuration
        scheduler->configureTemporalParameters(
            std::chrono::milliseconds(500),  // execution window
            std::chrono::milliseconds(200)   // default duration
        );
        
        // Test execution configuration
        scheduler->configureExecution(
            5,     // max concurrent
            true,  // use priority
            true   // enable temporal
        );
        
        // These should not throw and scheduler should remain functional
        TS_ASSERT(scheduler->isEnabled());
    }
    
    void test_enable_disable_scheduler()
    {
        TS_ASSERT(scheduler->isEnabled());
        
        scheduler->setEnabled(false);
        TS_ASSERT(!scheduler->isEnabled());
        
        // Disabled scheduler should not schedule new actions
        Handle test_action = atomspace->add_node(CONCEPT_NODE, "DisabledTest");
        bool scheduled = scheduler->scheduleImmediateAction(test_action);
        TS_ASSERT(!scheduled);
        
        // Re-enable
        scheduler->setEnabled(true);
        TS_ASSERT(scheduler->isEnabled());
        
        // Should be able to schedule again
        scheduled = scheduler->scheduleImmediateAction(test_action);
        TS_ASSERT(scheduled);
    }
    
    void test_cancel_all_actions()
    {
        // Schedule multiple actions
        for (int i = 0; i < 3; i++) {
            Handle action = atomspace->add_node(CONCEPT_NODE, "CancelTestAction" + std::to_string(i));
            scheduler->scheduleImmediateAction(action);
        }
        
        TS_ASSERT_EQUALS(scheduler->getPendingActionCount(), 3);
        
        // Cancel all actions
        scheduler->cancelAllActions();
        TS_ASSERT_EQUALS(scheduler->getPendingActionCount(), 0);
        TS_ASSERT_EQUALS(scheduler->getExecutingActionCount(), 0);
    }
    
    void test_statistics()
    {
        // Reset statistics
        scheduler->resetStatistics();
        
        // Get initial statistics
        std::string stats = scheduler->getSchedulingStatistics();
        TS_ASSERT(!stats.empty());
        TS_ASSERT(stats.find("actions_scheduled") != std::string::npos);
        
        // Schedule and process some actions
        Handle action = atomspace->add_node(CONCEPT_NODE, "StatsTestAction");
        scheduler->scheduleImmediateAction(action);
        scheduler->processScheduledActions();
        
        // Check updated statistics
        stats = scheduler->getSchedulingStatistics();
        TS_ASSERT(stats.find("\"actions_scheduled\": 1") != std::string::npos);
    }
    
    void test_status_info()
    {
        std::string status = scheduler->getStatusInfo();
        TS_ASSERT(!status.empty());
        TS_ASSERT(status.find("enabled") != std::string::npos);
        TS_ASSERT(status.find("max_concurrent") != std::string::npos);
        TS_ASSERT(status.find("statistics") != std::string::npos);
    }
    
    void test_invalid_action_handling()
    {
        // Try to schedule invalid action
        Handle invalid_action = Handle::UNDEFINED;
        bool scheduled = scheduler->scheduleImmediateAction(invalid_action);
        TS_ASSERT(!scheduled);
        TS_ASSERT_EQUALS(scheduler->getPendingActionCount(), 0);
    }
    
    void test_context_action_scheduling()
    {
        // Create action and context
        Handle action = atomspace->add_node(CONCEPT_NODE, "ContextTestAction");
        Handle context = atomspace->add_node(CONCEPT_NODE, "TestContext");
        
        // Schedule action with context
        bool scheduled = scheduler->scheduleImmediateAction(
            action,
            std::chrono::milliseconds(50),
            1,      // priority
            context // context
        );
        
        TS_ASSERT(scheduled);
        
        // Process the action
        bool processed = scheduler->processScheduledActions();
        TS_ASSERT(processed);
    }
};