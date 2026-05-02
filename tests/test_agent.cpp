/*
 * standalone/tests/test_agent.cpp
 *
 * Integration tests for the top-level Agent class.
 */
#include "test_runner.h"
#include "cog0/Agent.h"

using namespace cog0;

TEST(agent_initialises) {
    AgentConfig cfg;
    cfg.name = "test-agent";
    Agent a(cfg);
    ASSERT_EQ(a.name(), "test-agent");
    ASSERT_FALSE(a.isRunning());
}

TEST(set_goal_and_run_cycles) {
    Agent a;
    a.setGoal("explore", "Explore the environment", 0.9);
    a.runCycles(2);
    ASSERT_EQ(a.cognitiveLoop().cycleCount(), 2u);
    // Goal atom should be in store
    auto h = a.atomStore().getNode(AtomType::CONCEPT, "Goal:explore");
    ASSERT_TRUE(h != nullptr);
}

TEST(schedule_task_and_execute) {
    Agent a;
    bool done = false;
    a.scheduleTask("work", "do work", Priority::NORMAL, [&done]() {
        done = true;
        return true;
    });
    a.runCycles(1);
    ASSERT_TRUE(done);
}

TEST(percept_lands_in_store) {
    Agent a;
    a.addPercept("env", "sky is blue", 0.8);
    a.runCycles(1);
    auto h = a.atomStore().getNode(AtomType::CONCEPT, "Percept:sky is blue");
    ASSERT_TRUE(h != nullptr);
}

TEST(default_rules_fire) {
    Agent a;
    a.setGoal("test-goal");
    a.runCycles(2);
    // The default "goal-driven-assertion" rule should have added this atom
    auto h = a.atomStore().getNode(AtomType::CONCEPT, "AgentProperty:goal-driven");
    ASSERT_TRUE(h != nullptr);
}

TEST(high_salience_percept_triggers_attention_rule) {
    Agent a;
    a.addPercept("sensor", "urgent event", 0.95); // salience > 0.8
    a.runCycles(2);
    auto h = a.atomStore().getNode(AtomType::CONCEPT, "AttentionFlag:high-salience");
    ASSERT_TRUE(h != nullptr);
}

TEST(status_report_is_non_empty) {
    Agent a;
    a.setGoal("g");
    a.runCycles(1);
    std::string report = a.statusReport();
    ASSERT_FALSE(report.empty());
    ASSERT_TRUE(report.find("Agent Status") != std::string::npos);
}
