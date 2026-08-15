/*
 * opencog/agentzero/planning/GoalHierarchy.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Goal tree with dependency tracking for Agent-Zero planning (Phase 4).
 */

#ifndef _OPENCOG_AGENTZERO_PLANNING_GOAL_HIERARCHY_H
#define _OPENCOG_AGENTZERO_PLANNING_GOAL_HIERARCHY_H

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace planning {

/**
 * GoalHierarchy — hierarchical goal tree with prerequisite dependencies.
 *
 * Goals are CONCEPT_NODEs in AtomSpace. Parent/child relations use
 * INHERITANCE_LINK(subgoal, parent). Prerequisite edges are tracked in
 * memory and mirrored as EVALUATION_LINK(goal-depends-on, (dep, pre)).
 */
class GoalHierarchy
{
public:
    enum class GoalStatus {
        INACTIVE,
        ACTIVE,
        SATISFIED,
        FAILED,
        SUSPENDED,
        CANCELLED
    };

    enum class GoalPriority {
        CRITICAL = 1,
        HIGH     = 2,
        NORMAL   = 3,
        LOW      = 4,
        DEFERRED = 5
    };

    struct GoalNode {
        Handle goal_atom;
        Handle parent_goal;
        std::vector<Handle> subgoals;
        std::vector<Handle> dependencies; // prerequisites that must be SATISFIED
        GoalStatus status{GoalStatus::INACTIVE};
        GoalPriority priority{GoalPriority::NORMAL};
        float satisfaction_level{0.0f};
        std::chrono::steady_clock::time_point created_time;
        std::chrono::steady_clock::time_point deadline;
    };

    explicit GoalHierarchy(AtomSpacePtr atomspace);
    ~GoalHierarchy();

    // Lifecycle
    bool initialize();
    bool shutdown();
    bool isInitialized() const { return _initialized; }

    // Hierarchy mutation
    bool addGoal(const Handle& goal_atom,
                 const Handle& parent_goal = Handle::UNDEFINED,
                 GoalPriority priority = GoalPriority::NORMAL);
    bool removeGoal(const Handle& goal_atom, bool recursive = true);
    bool addSubgoal(const Handle& parent_goal, const Handle& subgoal);
    bool removeSubgoal(const Handle& parent_goal, const Handle& subgoal);

    // Status / satisfaction
    bool setGoalStatus(const Handle& goal_atom, GoalStatus status);
    GoalStatus getGoalStatus(const Handle& goal_atom) const;
    bool setGoalSatisfaction(const Handle& goal_atom, float satisfaction);
    float getGoalSatisfaction(const Handle& goal_atom) const;
    bool setGoalPriority(const Handle& goal_atom, GoalPriority priority);
    GoalPriority getGoalPriority(const Handle& goal_atom) const;

    // Queries
    std::vector<Handle> getRootGoals() const { return _root_goals; }
    std::vector<Handle> getSubgoals(const Handle& goal_atom) const;
    Handle getParentGoal(const Handle& goal_atom) const;
    std::vector<Handle> getAncestors(const Handle& goal_atom) const;
    std::vector<Handle> getLeafGoals(const Handle& root_goal) const;
    int getHierarchyDepth(const Handle& goal_atom) const;
    std::vector<Handle> getActiveGoals() const;
    std::vector<Handle> getGoalsByPriority(GoalPriority priority) const;
    std::vector<Handle> getGoalsByStatus(GoalStatus status) const;
    bool hasGoal(const Handle& goal_atom) const;
    size_t size() const { return _goal_nodes.size(); }

    // Dependencies (prerequisites beyond the parent/child tree)
    bool addGoalDependency(const Handle& dependent_goal,
                           const Handle& prerequisite_goal);
    bool removeGoalDependency(const Handle& dependent_goal,
                              const Handle& prerequisite_goal);
    std::vector<Handle> getGoalDependencies(const Handle& goal_atom) const;
    bool areGoalDependenciesSatisfied(const Handle& goal_atom) const;
    bool hasDependencyCycle(const Handle& from, const Handle& to) const;

    // Activation / planning selection
    bool activateGoal(const Handle& goal_atom);
    bool deactivateGoal(const Handle& goal_atom);
    Handle getNextGoalToPlan() const;

    // Hierarchical achievement: leaf = own TV; parent = 0.3*own + 0.7*avg(children)
    float calculateHierarchicalAchievement(const Handle& goal_atom) const;

    // Maintenance
    int updateGoalHierarchy();
    bool propagatePriority(const Handle& goal_atom, GoalPriority priority);

    // Configuration
    void setMaxHierarchyDepth(int max_depth) { _max_hierarchy_depth = max_depth; }
    void setMaxSubgoalsPerGoal(int max_subgoals) { _max_subgoals_per_goal = max_subgoals; }
    void configureFeatures(bool auto_activation, bool satisfaction_propagation) {
        _enable_automatic_activation = auto_activation;
        _enable_satisfaction_propagation = satisfaction_propagation;
    }

    Handle getGoalHierarchyContext() const { return _goal_hierarchy_context; }
    std::string getStatusInfo() const;

    static std::string statusToString(GoalStatus s);
    static std::string priorityToString(GoalPriority p);

private:
    AtomSpacePtr _atomspace;
    bool _initialized{false};

    std::map<Handle, GoalNode> _goal_nodes;
    std::vector<Handle> _root_goals;
    std::map<Handle, std::vector<Handle>> _dependency_graph;

    Handle _goal_hierarchy_context;
    Handle _goal_dependency_context;
    Handle _goal_satisfaction_context;

    int _max_hierarchy_depth{50};
    int _max_subgoals_per_goal{64};
    bool _enable_automatic_activation{true};
    bool _enable_satisfaction_propagation{true};

    void createGoalContexts();
    bool validateGoalStructure(const Handle& goal_atom) const;
    void propagateSatisfaction(const Handle& goal_atom);
    void mirrorGoalInAtomSpace(const GoalNode& node);
    void mirrorDependency(const Handle& dependent, const Handle& prerequisite);
    int depthFrom(const Handle& goal_atom, int current, int limit) const;
    bool dependsOnDFS(const Handle& current, const Handle& target,
                      std::map<Handle, bool>& visiting) const;
};

} // namespace planning
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_PLANNING_GOAL_HIERARCHY_H
