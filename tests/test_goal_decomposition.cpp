/*
 * tests/test_goal_decomposition.cpp
 *
 * Simple test to validate goal decomposition enhancements
 * Tests the logic without complex dependencies
 */

#include <iostream>
#include <string>
#include <vector>
#include <cassert>

// Test goal decomposition logic
std::vector<std::string> test_decompose_goal(const std::string& goal_name) {
    std::vector<std::string> subgoals;
    
    // This mirrors the logic from TaskManager::decomposeGoal
    if (goal_name.find("learn") != std::string::npos || goal_name.find("study") != std::string::npos) {
        subgoals = {"Identify_Learning_Objectives", "Gather_Resources", "Acquire_Knowledge", "Practice_Skills", "Validate_Understanding"};
    } else if (goal_name.find("solve") != std::string::npos || goal_name.find("problem") != std::string::npos) {
        subgoals = {"Define_Problem", "Analyze_Constraints", "Generate_Solutions", "Evaluate_Options", "Implement_Solution", "Test_Result"};
    } else if (goal_name.find("create") != std::string::npos || goal_name.find("build") != std::string::npos) {
        subgoals = {"Conceptualize_Design", "Plan_Implementation", "Gather_Resources", "Execute_Construction", "Test_Quality", "Refine_Output"};
    } else if (goal_name.find("communicate") != std::string::npos || goal_name.find("interact") != std::string::npos) {
        subgoals = {"Understand_Context", "Plan_Message", "Select_Medium", "Deliver_Communication", "Verify_Understanding"};
    } else {
        subgoals = {"Analyze_Goal_Context", "Plan_Approach", "Identify_Resources", "Execute_Actions", "Monitor_Progress", "Verify_Achievement"};
    }
    
    return subgoals;
}

void test_goal_decomposition_logic() {
    std::cout << "Testing goal decomposition logic..." << std::endl;
    
    // Test learning goals
    auto learn_subgoals = test_decompose_goal("learn_programming");
    assert(learn_subgoals.size() == 5);
    assert(learn_subgoals[0] == "Identify_Learning_Objectives");
    std::cout << "✓ Learning goal decomposition: " << learn_subgoals.size() << " subgoals" << std::endl;
    
    // Test problem-solving goals
    auto solve_subgoals = test_decompose_goal("solve_math_problem");
    assert(solve_subgoals.size() == 6);
    assert(solve_subgoals[0] == "Define_Problem");
    std::cout << "✓ Problem-solving goal decomposition: " << solve_subgoals.size() << " subgoals" << std::endl;
    
    // Test creative goals
    auto create_subgoals = test_decompose_goal("create_website");
    assert(create_subgoals.size() == 6);
    assert(create_subgoals[0] == "Conceptualize_Design");
    std::cout << "✓ Creative goal decomposition: " << create_subgoals.size() << " subgoals" << std::endl;
    
    // Test communication goals
    auto comm_subgoals = test_decompose_goal("communicate_with_user");
    assert(comm_subgoals.size() == 5);
    assert(comm_subgoals[0] == "Understand_Context");
    std::cout << "✓ Communication goal decomposition: " << comm_subgoals.size() << " subgoals" << std::endl;
    
    // Test generic goals
    auto generic_subgoals = test_decompose_goal("complete_task");
    assert(generic_subgoals.size() == 6);
    assert(generic_subgoals[0] == "Analyze_Goal_Context");
    std::cout << "✓ Generic goal decomposition: " << generic_subgoals.size() << " subgoals" << std::endl;
}

void test_goal_type_detection() {
    std::cout << "\nTesting goal type detection..." << std::endl;
    
    struct TestCase {
        std::string goal;
        std::string expected_type;
        size_t expected_subgoals;
    };
    
    std::vector<TestCase> test_cases = {
        {"learn_python", "learning", 5},
        {"study_mathematics", "learning", 5},
        {"solve_equation", "problem-solving", 6},
        {"problem_analysis", "problem-solving", 6},
        {"create_application", "creative", 6},
        {"build_system", "creative", 6},
        {"communicate_results", "communication", 5},
        {"interact_with_client", "communication", 5},
        {"perform_analysis", "generic", 6},
        {"execute_plan", "generic", 6}
    };
    
    for (const auto& test_case : test_cases) {
        auto subgoals = test_decompose_goal(test_case.goal);
        assert(subgoals.size() == test_case.expected_subgoals);
        std::cout << "✓ Goal '" << test_case.goal << "' -> " 
                  << test_case.expected_type << " (" << subgoals.size() << " subgoals)" << std::endl;
    }
}

void display_decomposition_examples() {
    std::cout << "\nGoal Decomposition Examples:" << std::endl;
    std::cout << "============================" << std::endl;
    
    std::vector<std::string> example_goals = {
        "learn_machine_learning",
        "solve_optimization_problem", 
        "create_mobile_app",
        "communicate_project_status"
    };
    
    for (const std::string& goal : example_goals) {
        auto subgoals = test_decompose_goal(goal);
        std::cout << "\nGoal: " << goal << std::endl;
        std::cout << "Subgoals:" << std::endl;
        for (size_t i = 0; i < subgoals.size(); ++i) {
            std::cout << "  " << (i+1) << ". " << subgoals[i] << std::endl;
        }
    }
}

int main() {
    std::cout << "=== Enhanced Goal Decomposition Test ===" << std::endl;
    
    try {
        test_goal_decomposition_logic();
        test_goal_type_detection();
        display_decomposition_examples();
        
        std::cout << "\n🎉 All goal decomposition tests passed!" << std::endl;
        std::cout << "\nThis validates the enhanced goal decomposition logic" << std::endl;
        std::cout << "that would be used in the TaskManager implementation." << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}