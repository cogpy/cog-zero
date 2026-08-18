/*
 * tests/PolicyOptimizerTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Standalone compilation & basic-functionality test for PolicyOptimizer
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

#include "../include/opencog/agentzero/ExperienceManager.h"
#include "../include/opencog/agentzero/PolicyOptimizer.h"

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    logger().set_level(Logger::WARN);

    std::cout << "=== PolicyOptimizer Simple Test ===" << std::endl;

    try {
        auto atomspace = std::make_shared<AtomSpace>();
        PolicyOptimizerConfig cfg;
        cfg.population_size  = 10;
        cfg.max_generations  = 5;
        cfg.mutation_rate    = 0.2;
        cfg.crossover_rate   = 0.6;
        cfg.elite_fraction   = 0.2;
        cfg.tournament_size  = 3;
        PolicyOptimizer opt(atomspace, cfg);

        // 1. Initial state
        std::cout << "\n1. Initial state..." << std::endl;
        assert(opt.getBestPolicy() == Handle::UNDEFINED);
        std::cout << "   PASS" << std::endl;

        // 2. Seed a policy
        std::cout << "\n2. Seed population..." << std::endl;
        Handle seed = atomspace->add_node(CONCEPT_NODE, "SeedPolicy");
        opt.seedPopulation({seed});
        assert(!opt.getPopulation().empty());
        std::cout << "   PASS (" << opt.getPopulation().size() << " candidates)" << std::endl;

        // 3. Evaluate a policy
        std::cout << "\n3. Policy evaluation..." << std::endl;
        std::vector<Experience> exps;
        Handle ctx  = atomspace->add_node(CONCEPT_NODE, "ctx");
        Handle task = atomspace->add_node(CONCEPT_NODE, "task");
        Handle out  = atomspace->add_node(CONCEPT_NODE, "outcome");
        for (int i = 0; i < 5; ++i) {
            Experience e;
            e.type      = (i % 2 == 0) ? ExperienceType::SUCCESS : ExperienceType::FAILURE;
            e.context   = ctx;
            e.task      = task;
            e.outcome   = out;
            e.importance = 0.7;
            exps.push_back(e);
        }
        double score = opt.evaluatePolicy(seed, exps);
        assert(score >= 0.0 && score <= 1.0);
        std::cout << "   PASS (score=" << score << ")" << std::endl;

        // 4. Custom evaluator
        std::cout << "\n4. Custom evaluator..." << std::endl;
        auto custom_eval = [](const Handle&, const std::vector<Experience>& e) -> double {
            return static_cast<double>(e.size()) * 0.1;
        };
        double custom_score = opt.evaluatePolicy(seed, exps, custom_eval);
        assert(custom_score == static_cast<double>(exps.size()) * 0.1);
        std::cout << "   PASS (score=" << custom_score << ")" << std::endl;

        // 5. Optimise a policy
        std::cout << "\n5. Policy optimisation..." << std::endl;
        Handle best = opt.optimizePolicy(seed, exps);
        assert(best != Handle::UNDEFINED);
        std::cout << "   PASS" << std::endl;

        // 6. Best policy is accessible
        std::cout << "\n6. Best policy retrieval..." << std::endl;
        Handle bp = opt.getBestPolicy();
        assert(bp != Handle::UNDEFINED);
        std::cout << "   PASS" << std::endl;

        // 7. Statistics
        std::cout << "\n7. Statistics..." << std::endl;
        auto stats = opt.getOptimizationStatistics();
        assert(stats.at("population_size") >= 1.0);
        assert(stats.at("current_generation") >= 1.0);
        std::cout << "   PASS (generations=" << stats.at("current_generation") << ")" << std::endl;

        std::cout << "\n=== All tests passed ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
