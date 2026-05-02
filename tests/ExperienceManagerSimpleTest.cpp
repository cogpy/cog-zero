/*
 * tests/ExperienceManagerSimpleTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Standalone compilation & basic-functionality test for ExperienceManager
 * Part of Phase 5: Continuous Learning & Adaptation
 */

#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/util/Logger.h>

#include "../include/opencog/agentzero/ExperienceManager.h"

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    logger().set_level(Logger::WARN);

    std::cout << "=== ExperienceManager Simple Test ===" << std::endl;

    try {
        // 1. Initialisation
        std::cout << "\n1. Initialisation..." << std::endl;
        auto atomspace = std::make_shared<AtomSpace>();
        ExperienceManager mgr(atomspace, /*max=*/100);
        assert(mgr.isInitialized());
        assert(mgr.getExperienceCount() == 0);
        std::cout << "   PASS" << std::endl;

        // 2. Record a basic experience
        std::cout << "\n2. Recording experiences..." << std::endl;
        Handle ctx  = atomspace->add_node(CONCEPT_NODE, "indoor_env");
        Handle task = atomspace->add_node(CONCEPT_NODE, "navigate");
        Handle out  = atomspace->add_node(CONCEPT_NODE, "reached_goal");

        Handle h = mgr.recordExperience(ExperienceType::ACTION_OUTCOME, ctx, task, out, 0.8);
        assert(h != Handle::UNDEFINED);
        assert(mgr.getExperienceCount() == 1);
        std::cout << "   PASS" << std::endl;

        // 3. Record multiple types
        std::cout << "\n3. Multiple types..." << std::endl;
        mgr.recordExperience(ExperienceType::SUCCESS,  ctx, task, out, 0.9);
        mgr.recordExperience(ExperienceType::FAILURE,  ctx, task, out, 0.3);
        mgr.recordExperience(ExperienceType::DISCOVERY, ctx, task, out, 0.7);
        assert(mgr.getExperienceCount() == 4);
        std::cout << "   PASS" << std::endl;

        // 4. Retrieve by type
        std::cout << "\n4. Retrieve by type..." << std::endl;
        auto successes = mgr.getExperiencesByType(ExperienceType::SUCCESS);
        assert(successes.size() == 1);
        auto failures = mgr.getExperiencesByType(ExperienceType::FAILURE);
        assert(failures.size() == 1);
        std::cout << "   PASS" << std::endl;

        // 5. Retrieve recent experiences
        std::cout << "\n5. Recent experiences..." << std::endl;
        auto recent = mgr.getRecentExperiences(std::chrono::hours(1));
        assert(recent.size() == 4);
        std::cout << "   PASS" << std::endl;

        // 6. Retrieve by context
        std::cout << "\n6. Context retrieval..." << std::endl;
        auto ctx_exps = mgr.getExperiencesByContext(ctx);
        assert(ctx_exps.size() == 4);
        std::cout << "   PASS" << std::endl;

        // 7. Similar experience search
        std::cout << "\n7. Similar experience search..." << std::endl;
        Experience target;
        target.type    = ExperienceType::SUCCESS;
        target.context = ctx;
        target.task    = task;
        auto similar = mgr.findSimilarExperiences(target, 5);
        assert(!similar.empty());
        std::cout << "   PASS (" << similar.size() << " found)" << std::endl;

        // 8. Update importance
        std::cout << "\n8. Update importance..." << std::endl;
        bool updated = mgr.updateExperienceImportance(h, 0.95);
        assert(updated);
        std::cout << "   PASS" << std::endl;

        // 9. Statistics
        std::cout << "\n9. Statistics..." << std::endl;
        auto stats = mgr.getExperienceStatistics();
        assert(stats.at("total_experiences") == 4.0);
        assert(stats.at("success_count") == 1.0);
        assert(stats.at("failure_count") == 1.0);
        std::cout << "   PASS (total=" << stats.at("total_experiences") << ")" << std::endl;

        // 10. Memory consolidation
        std::cout << "\n10. Memory consolidation..." << std::endl;
        mgr.setMaxExperiences(2);
        mgr.consolidateMemory();
        assert(mgr.getExperienceCount() <= 2);
        std::cout << "   PASS" << std::endl;

        // 11. Type conversion utilities
        std::cout << "\n11. Type conversion utilities..." << std::endl;
        assert(ExperienceManager::experienceTypeToString(ExperienceType::SUCCESS) == "SUCCESS");
        assert(ExperienceManager::stringToExperienceType("FAILURE") == ExperienceType::FAILURE);
        std::cout << "   PASS" << std::endl;

        std::cout << "\n=== All tests passed ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
