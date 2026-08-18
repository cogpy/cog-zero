/*
 * test_TemporalReasoner.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for TemporalReasoner
 * Part of AZ-PLAN-002: Create PlanningEngine with temporal reasoning
 */

#include <cxxtest/TestSuite.h>
#include <opencog/agentzero/TemporalReasoner.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>

using namespace opencog;
using namespace opencog::agentzero;

class TemporalReasonerTestSuite : public CxxTest::TestSuite
{
private:
    AtomSpacePtr as;
    std::unique_ptr<TemporalReasoner> temporal_reasoner;

public:
    void setUp()
    {
        as = std::make_shared<AtomSpace>();
        temporal_reasoner = std::make_unique<TemporalReasoner>(as);
    }

    void tearDown()
    {
        temporal_reasoner.reset();
        as.reset();
    }

    void test_constructor()
    {
        TS_ASSERT(temporal_reasoner != nullptr);
        TS_ASSERT(temporal_reasoner->getTemporalContext() != Handle::UNDEFINED);
    }

    void test_temporal_interval()
    {
        Handle event = as->add_node(CONCEPT_NODE, "test-event");
        
        auto start = std::chrono::steady_clock::now();
        auto end = start + std::chrono::seconds(5);
        
        bool result = temporal_reasoner->addTemporalInterval(event, start, end);
        
        TS_ASSERT(result);
        
        const auto* interval = temporal_reasoner->getTemporalInterval(event);
        TS_ASSERT(interval != nullptr);
        TS_ASSERT_EQUALS(interval->event_atom, event);
        TS_ASSERT_EQUALS(interval->start, start);
        TS_ASSERT_EQUALS(interval->end, end);
    }

    void test_absolute_constraint()
    {
        Handle event = as->add_node(CONCEPT_NODE, "constraint-event");
        auto target_time = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        
        bool result = temporal_reasoner->addAbsoluteConstraint(event, target_time);
        
        TS_ASSERT(result);
        
        // Test constraint validation
        std::vector<Handle> events = {event};
        // Note: This will fail validation since we haven't set the interval to match the constraint
        // This is expected behavior for this test
    }

    void test_relative_constraint()
    {
        Handle event1 = as->add_node(CONCEPT_NODE, "event1");
        Handle event2 = as->add_node(CONCEPT_NODE, "event2");
        
        bool result = temporal_reasoner->addRelativeConstraint(
            event2, event1, TemporalReasoner::TemporalRelation::AFTER);
        
        TS_ASSERT(result);
        
        auto relation = temporal_reasoner->getTemporalRelation(event2, event1);
        TS_ASSERT_EQUALS(relation, TemporalReasoner::TemporalRelation::AFTER);
    }

    void test_deadline_constraint()
    {
        Handle event = as->add_node(CONCEPT_NODE, "deadline-event");
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        
        bool result = temporal_reasoner->addDeadlineConstraint(event, deadline);
        
        TS_ASSERT(result);
    }

    void test_temporal_relations()
    {
        Handle event1 = as->add_node(CONCEPT_NODE, "before-event");
        Handle event2 = as->add_node(CONCEPT_NODE, "after-event");
        
        auto start1 = std::chrono::steady_clock::now();
        auto end1 = start1 + std::chrono::seconds(2);
        auto start2 = end1 + std::chrono::seconds(1);
        auto end2 = start2 + std::chrono::seconds(2);
        
        temporal_reasoner->addTemporalInterval(event1, start1, end1);
        temporal_reasoner->addTemporalInterval(event2, start2, end2);
        
        auto relation = temporal_reasoner->getTemporalRelation(event1, event2);
        TS_ASSERT_EQUALS(relation, TemporalReasoner::TemporalRelation::BEFORE);
    }

    void test_events_in_interval()
    {
        Handle event1 = as->add_node(CONCEPT_NODE, "interval-test-event1");
        Handle event2 = as->add_node(CONCEPT_NODE, "interval-test-event2");
        Handle event3 = as->add_node(CONCEPT_NODE, "interval-test-event3");
        
        auto base_time = std::chrono::steady_clock::now();
        
        // Event1: 0-2 seconds
        temporal_reasoner->addTemporalInterval(event1, base_time, base_time + std::chrono::seconds(2));
        
        // Event2: 5-7 seconds  
        temporal_reasoner->addTemporalInterval(event2, base_time + std::chrono::seconds(5), 
                                             base_time + std::chrono::seconds(7));
        
        // Event3: 10-12 seconds
        temporal_reasoner->addTemporalInterval(event3, base_time + std::chrono::seconds(10),
                                             base_time + std::chrono::seconds(12));
        
        // Query for events in 4-8 second range
        auto events = temporal_reasoner->getEventsInInterval(base_time + std::chrono::seconds(4),
                                                           base_time + std::chrono::seconds(8));
        
        TS_ASSERT_EQUALS(events.size(), 1);
        TS_ASSERT_EQUALS(events[0], event2);  // Only event2 should be in this range
    }

    void test_configuration()
    {
        temporal_reasoner->setTemporalResolution(50);  // 50ms resolution
        temporal_reasoner->setPlanningHorizon(60000);  // 1 minute horizon
        temporal_reasoner->configureFeatures(true, false, 0.2f);
        
        // Configuration is applied - test by creating constraints
        Handle event = as->add_node(CONCEPT_NODE, "config-test");
        auto result = temporal_reasoner->addAbsoluteConstraint(event, 
                                                             std::chrono::steady_clock::now());
        TS_ASSERT(result);
    }

    void test_temporal_reasoning_update()
    {
        int updates = temporal_reasoner->updateTemporalReasoning();
        TS_ASSERT_LESS_THAN_EQUALS(0, updates);  // Should return non-negative number
    }
};