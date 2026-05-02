/*
 * tests/IntegrationTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Integration tests for Agent-Zero Learning module
 * Tests interaction between all learning components
 */

#include <iostream>
#include <cassert>
#include <memory>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/SkillAcquisition.h"
#include "opencog/agentzero/ExperienceManager.h"
#include "opencog/agentzero/PolicyOptimizer.h"
#include "opencog/agentzero/MetaLearning.h"

using namespace opencog;
using namespace opencog::agentzero;

/**
 * Integration test class for Agent-Zero Learning components
 */
class IntegrationTest
{
private:
    AtomSpacePtr _atomspace;
    std::unique_ptr<SkillAcquisition> _skill_acquisition;
    std::unique_ptr<ExperienceManager> _experience_manager;
    std::unique_ptr<PolicyOptimizer> _policy_optimizer;
    std::unique_ptr<MetaLearning> _meta_learning;

public:
    IntegrationTest()
        : _atomspace(std::make_shared<AtomSpace>())
        , _skill_acquisition(std::make_unique<SkillAcquisition>(_atomspace))
        , _experience_manager(std::make_unique<ExperienceManager>(_atomspace))
        , _policy_optimizer(std::make_unique<PolicyOptimizer>(_atomspace))
        , _meta_learning(std::make_unique<MetaLearning>(_atomspace))
    {
        logger().set_level(Logger::INFO);
        logger().set_print_to_stdout_flag(true);
    }

    void runAllTests()
    {
        std::cout << "Running Agent-Zero Learning integration tests..." << std::endl;
        
        testAtomSpaceIntegration();
        testExperienceBasedLearning();
        testPolicyOptimizedSkills();
        testMetaLearningAdaptation();
        testCompleteWorkflow();
        
        std::cout << "All integration tests passed!" << std::endl;
    }

private:
    void testAtomSpaceIntegration()
    {
        std::cout << "Testing AtomSpace integration..." << std::endl;
        
        // Count initial atoms
        size_t initial_atom_count = _atomspace->get_size();
        
        // Create a skill - should add atoms to AtomSpace
        std::vector<Handle> experience_data;
        experience_data.push_back(_atomspace->add_node(CONCEPT_NODE, "IntegrationExp1"));
        experience_data.push_back(_atomspace->add_node(CONCEPT_NODE, "IntegrationExp2"));
        
        Handle skill = _skill_acquisition->learnSkill(
            "IntegrationSkill",
            SkillAcquisition::SkillType::COGNITIVE,
            SkillAcquisition::LearningStrategy::REINFORCEMENT,
            experience_data
        );
        
        // Verify atoms were added
        size_t new_atom_count = _atomspace->get_size();
        assert(new_atom_count > initial_atom_count);
        
        // Verify skill atom exists in AtomSpace
        assert(skill != Handle::UNDEFINED);
        assert(_atomspace->get_atom(skill) == skill);
        
        std::cout << "✓ AtomSpace integration test passed" << std::endl;
    }

    void testExperienceBasedLearning()
    {
        std::cout << "Testing experience-based learning..." << std::endl;
        
        // Record experiences
        Handle exp1 = _experience_manager->recordExperience(
            ExperienceManager::ExperienceType::SUCCESS,
            _atomspace->add_node(CONCEPT_NODE, "SuccessfulAction")
        );
        
        Handle exp2 = _experience_manager->recordExperience(
            ExperienceManager::ExperienceType::PROBLEM_SOLVING,
            _atomspace->add_node(CONCEPT_NODE, "ProblemSolved")
        );
        
        assert(exp1 != Handle::UNDEFINED);
        assert(exp2 != Handle::UNDEFINED);
        
        // Use experiences for skill learning
        std::vector<Handle> experiences = {exp1, exp2};
        Handle experience_skill = _skill_acquisition->learnSkill(
            "ExperienceBasedSkill",
            SkillAcquisition::SkillType::PROBLEM_SOLVING,
            SkillAcquisition::LearningStrategy::REINFORCEMENT,
            experiences
        );
        
        assert(experience_skill != Handle::UNDEFINED);
        assert(_skill_acquisition->hasSkill("ExperienceBasedSkill"));
        
        // Verify experience retrieval works
        std::vector<Handle> success_experiences = 
            _experience_manager->getExperiencesByType(ExperienceManager::ExperienceType::SUCCESS);
        assert(!success_experiences.empty());
        
        std::cout << "✓ Experience-based learning test passed" << std::endl;
    }

    void testPolicyOptimizedSkills()
    {
        std::cout << "Testing policy-optimized skills..." << std::endl;
        
        // Create training data
        std::vector<Handle> training_data;
        training_data.push_back(_atomspace->add_node(CONCEPT_NODE, "TrainingData1"));
        training_data.push_back(_atomspace->add_node(CONCEPT_NODE, "TrainingData2"));
        training_data.push_back(_atomspace->add_node(CONCEPT_NODE, "TrainingData3"));
        
        // Create initial policy structure
        HandleSeq initial_structure;
        initial_structure.push_back(_atomspace->add_node(CONCEPT_NODE, "PolicyComponent1"));
        initial_structure.push_back(_atomspace->add_node(CONCEPT_NODE, "PolicyComponent2"));
        
        // Optimize policy
        Handle optimized_policy = _policy_optimizer->optimizePolicy(
            "TestPolicy",
            PolicyOptimizer::PolicyType::ACTION_SELECTION,
            PolicyOptimizer::OptimizationObjective::MAXIMIZE_EFFICIENCY,
            initial_structure,
            training_data
        );
        
        assert(optimized_policy != Handle::UNDEFINED);
        
        // Use optimized policy components in skill learning
        std::vector<Handle> policy_experiences = {optimized_policy};
        Handle policy_skill = _skill_acquisition->learnSkill(
            "PolicyOptimizedSkill",
            SkillAcquisition::SkillType::PROCEDURAL,
            SkillAcquisition::LearningStrategy::REINFORCEMENT,
            policy_experiences
        );
        
        assert(policy_skill != Handle::UNDEFINED);
        
        // Test policy fitness evaluation
        double fitness = _policy_optimizer->evaluatePolicyFitness(optimized_policy, training_data);
        assert(fitness >= 0.0 && fitness <= 1.0);
        
        std::cout << "✓ Policy-optimized skills test passed" << std::endl;
    }

    void testMetaLearningAdaptation()
    {
        std::cout << "Testing meta-learning adaptation..." << std::endl;
        
        Handle skill = _skill_acquisition->getSkill("IntegrationSkill");
        assert(skill != Handle::UNDEFINED);
        
        // Test meta-learning strategy adaptation
        std::vector<Handle> learning_context;
        learning_context.push_back(_atomspace->add_node(CONCEPT_NODE, "LearningContext1"));
        
        MetaLearning::StrategyAdaptation adaptation = 
            _meta_learning->adaptLearningStrategy(skill, 1, learning_context);
        
        // Should return a valid adaptation strategy
        assert(static_cast<int>(adaptation) >= 0);
        
        // Test parameter optimization
        std::map<std::string, double> current_params;
        current_params["learning_rate"] = 0.1;
        current_params["exploration_rate"] = 0.3;
        
        std::vector<double> performance_feedback = {0.4, 0.5, 0.6, 0.7};
        
        std::map<std::string, double> optimized_params = 
            _meta_learning->optimizeLearningParameters(
                "test_context",
                current_params,
                performance_feedback
            );
        
        assert(!optimized_params.empty());
        assert(optimized_params.find("learning_rate") != optimized_params.end());
        
        // Test adaptation effectiveness evaluation
        _meta_learning->evaluateAdaptationEffectiveness(
            skill, adaptation, 0.5, 0.7
        );
        
        std::cout << "✓ Meta-learning adaptation test passed" << std::endl;
    }

    void testCompleteWorkflow()
    {
        std::cout << "Testing complete learning workflow..." << std::endl;
        
        // Step 1: Record initial experiences
        Handle workflow_exp = _experience_manager->recordExperience(
            ExperienceManager::ExperienceType::ACTION_OUTCOME,
            _atomspace->add_node(CONCEPT_NODE, "WorkflowAction")
        );
        
        // Step 2: Learn initial skill from experience
        std::vector<Handle> workflow_experiences = {workflow_exp};
        Handle workflow_skill = _skill_acquisition->learnSkill(
            "WorkflowSkill",
            SkillAcquisition::SkillType::ADAPTIVE,
            SkillAcquisition::LearningStrategy::EXPLORATORY,
            workflow_experiences
        );
        
        // Step 3: Get initial proficiency
        SkillAcquisition::ProficiencyLevel initial_proficiency = 
            _skill_acquisition->getSkillProficiency(workflow_skill);
        
        // Step 4: Practice skill with new experiences
        Handle practice_exp = _experience_manager->recordExperience(
            ExperienceManager::ExperienceType::SUCCESS,
            _atomspace->add_node(CONCEPT_NODE, "PracticeSuccess")
        );
        
        std::vector<Handle> practice_data = {practice_exp};
        SkillAcquisition::ProficiencyLevel new_proficiency = 
            _skill_acquisition->practiceSkill(workflow_skill, practice_data);
        
        // Step 5: Apply skill to new task
        std::vector<Handle> task_context;
        task_context.push_back(_atomspace->add_node(CONCEPT_NODE, "WorkflowTask"));
        
        auto application_result = _skill_acquisition->applySkill(workflow_skill, task_context);
        assert(application_result.first == true);
        
        // Step 6: Use meta-learning to optimize
        std::vector<Handle> meta_context = {workflow_skill};
        MetaLearning::StrategyAdaptation meta_adaptation = 
            _meta_learning->adaptLearningStrategy(workflow_skill, 2, meta_context);
        
        // Step 7: Transfer skill to new domain
        std::vector<Handle> transfer_rules;
        transfer_rules.push_back(_atomspace->add_node(CONCEPT_NODE, "TransferRule"));
        
        Handle transferred_workflow_skill = _skill_acquisition->transferSkill(
            workflow_skill,
            "TransferredWorkflowSkill",
            transfer_rules
        );
        
        assert(transferred_workflow_skill != Handle::UNDEFINED);
        
        // Verify complete workflow results
        assert(_skill_acquisition->getLearnedSkills().size() >= 4); // All created skills
        assert(_experience_manager->getTotalExperiences() >= 2); // All recorded experiences
        
        // Get final statistics
        std::map<std::string, double> skill_stats = _skill_acquisition->getLearningStatistics();
        std::map<std::string, double> meta_stats = _meta_learning->getMetaLearningStatistics();
        std::map<std::string, double> policy_stats = _policy_optimizer->getOptimizationStatistics();
        
        assert(!skill_stats.empty());
        assert(!meta_stats.empty());
        assert(!policy_stats.empty());
        
        std::cout << "✓ Complete workflow test passed" << std::endl;
        
        // Print summary statistics
        std::cout << "Final Statistics:" << std::endl;
        std::cout << "  Total Skills: " << skill_stats["total_skills"] << std::endl;
        std::cout << "  Total Experiences: " << _experience_manager->getTotalExperiences() << std::endl;
        std::cout << "  Meta-learning Contexts: " << meta_stats["total_learning_contexts"] << std::endl;
        std::cout << "  Total Policies: " << policy_stats["total_policies"] << std::endl;
    }
};

int main()
{
    try {
        IntegrationTest test;
        test.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Integration test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}