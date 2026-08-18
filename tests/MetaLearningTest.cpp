/*
 * tests/MetaLearningTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Standalone compilation & basic-functionality test for MetaLearning
 * Part of Phase 5: Continuous Learning & Adaptation
 */

#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/util/Logger.h>

#include "../include/opencog/agentzero/MetaLearning.h"

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    logger().set_level(Logger::WARN);

    std::cout << "=== MetaLearning Simple Test ===" << std::endl;

    try {
        auto atomspace = std::make_shared<AtomSpace>();
        MetaLearningConfig cfg;
        cfg.min_samples_for_adaptation = 3;
        cfg.adaptation_window          = 5;
        cfg.adaptation_threshold       = 0.05;
        cfg.learning_rate              = 0.2;
        MetaLearning ml(atomspace, cfg);

        // 1. Initial state
        std::cout << "\n1. Initial state..." << std::endl;
        assert(ml.getCurrentStrategy() == LearningStrategy::EXPLORATION);
        assert(ml.getExperienceManager().getExperienceCount() == 0);
        assert(ml.getSkillAcquisition().getSkillCount() == 0);
        std::cout << "   PASS" << std::endl;

        // 2. Process experiences
        std::cout << "\n2. Process experiences..." << std::endl;
        Handle ctx  = atomspace->add_node(CONCEPT_NODE, "context_A");
        Handle task = atomspace->add_node(CONCEPT_NODE, "task_A");
        Handle out  = atomspace->add_node(CONCEPT_NODE, "outcome_success");

        for (int i = 0; i < 5; ++i) {
            Handle h = ml.processExperience(
                ExperienceType::SUCCESS, ctx, task, out, 1.0, 0.8);
            assert(h != Handle::UNDEFINED);
        }
        assert(ml.getExperienceManager().getExperienceCount() == 5);
        std::cout << "   PASS (5 experiences processed)" << std::endl;

        // 3. Record strategy performance
        std::cout << "\n3. Strategy performance tracking..." << std::endl;
        ml.recordStrategyPerformance(LearningStrategy::REINFORCEMENT, true,  1.0);
        ml.recordStrategyPerformance(LearningStrategy::REINFORCEMENT, true,  0.8);
        ml.recordStrategyPerformance(LearningStrategy::REINFORCEMENT, false, -0.2);

        auto perfs = ml.getStrategyPerformances();
        auto it = perfs.find(LearningStrategy::REINFORCEMENT);
        assert(it != perfs.end());
        assert(it->second.episode_count == 3);
        std::cout << "   PASS (success_rate=" << it->second.success_rate << ")" << std::endl;

        // 4. Strategy selection
        std::cout << "\n4. Strategy selection..." << std::endl;
        LearningStrategy sel = ml.selectStrategy(ctx);
        std::cout << "   PASS (selected=" << MetaLearning::strategyToString(sel) << ")" << std::endl;

        // 5. Adapt learning parameters
        std::cout << "\n5. Adapt learning parameters..." << std::endl;
        // Record enough samples to trigger adaptation
        for (int i = 0; i < cfg.min_samples_for_adaptation; ++i) {
            ml.recordStrategyPerformance(LearningStrategy::REINFORCEMENT, true, 0.9);
        }
        ml.adaptLearningParameters(); // Should not throw
        std::cout << "   PASS" << std::endl;

        // 6. Statistics
        std::cout << "\n6. Statistics..." << std::endl;
        auto stats = ml.getMetaLearningStatistics();
        assert(stats.find("total_experiences") != stats.end());
        assert(stats.at("total_experiences") >= 5.0);
        std::cout << "   PASS (total_experiences=" << stats.at("total_experiences") << ")" << std::endl;

        // 7. Strategy name conversion
        std::cout << "\n7. Strategy name conversion..." << std::endl;
        assert(MetaLearning::strategyToString(LearningStrategy::REINFORCEMENT) == "REINFORCEMENT");
        assert(MetaLearning::strategyToString(LearningStrategy::META_ADAPTIVE)  == "META_ADAPTIVE");
        std::cout << "   PASS" << std::endl;

        // 8. Sub-component access
        std::cout << "\n8. Sub-component access..." << std::endl;
        const auto& em = ml.getExperienceManager();
        assert(em.isInitialized());
        const auto& popt = ml.getPolicyOptimizer();
        // Just accessing the config is enough to prove the accessor works
        (void)popt.getConfig();
        std::cout << "   PASS" << std::endl;

        std::cout << "\n=== All tests passed ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
