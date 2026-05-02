/*
 * tests/SkillAcquisitionTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Standalone compilation & basic-functionality test for SkillAcquisition
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
#include "../include/opencog/agentzero/SkillAcquisition.h"

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    logger().set_level(Logger::WARN);

    std::cout << "=== SkillAcquisition Simple Test ===" << std::endl;

    try {
        auto atomspace = std::make_shared<AtomSpace>();
        SkillAcquisitionConfig cfg;
        cfg.min_demonstrations = 2;
        cfg.min_success_rate   = 0.4;
        SkillAcquisition sa(atomspace, cfg);

        // 1. Initial state
        std::cout << "\n1. Initial state..." << std::endl;
        assert(sa.getSkillCount() == 0);
        std::cout << "   PASS" << std::endl;

        // 2. Learn a skill from success experiences
        std::cout << "\n2. Learn from experiences..." << std::endl;
        Handle ctx  = atomspace->add_node(CONCEPT_NODE, "context_nav");
        Handle task = atomspace->add_node(CONCEPT_NODE, "task_navigate");
        Handle out  = atomspace->add_node(CONCEPT_NODE, "outcome_success");

        std::vector<Experience> experiences;
        for (int i = 0; i < 4; ++i) {
            Experience exp;
            exp.type      = ExperienceType::SUCCESS;
            exp.context   = ctx;
            exp.task      = task;
            exp.outcome   = out;
            exp.importance = 0.8;
            experiences.push_back(exp);
        }

        auto new_skills = sa.learnFromExperiences(experiences);
        assert(!new_skills.empty());
        std::cout << "   PASS (" << sa.getSkillCount() << " skill(s) acquired)" << std::endl;

        // 3. Retrieve applicable skills
        std::cout << "\n3. Applicable skills..." << std::endl;
        auto applicable = sa.getApplicableSkills(ctx);
        assert(!applicable.empty());
        std::cout << "   PASS (" << applicable.size() << " applicable)" << std::endl;

        // 4. Refine a skill
        std::cout << "\n4. Skill refinement..." << std::endl;
        Handle skill_handle = new_skills[0];
        for (int i = 0; i < 10; ++i) {
            sa.refineSkill(skill_handle, true, ctx);
        }
        Skill refined = sa.getSkill(skill_handle);
        assert(refined.success_rate > 0.5);
        std::cout << "   PASS (success_rate=" << refined.success_rate << ")" << std::endl;

        // 5. Transfer a skill
        std::cout << "\n5. Skill transfer..." << std::endl;
        Handle new_ctx = atomspace->add_node(CONCEPT_NODE, "context_outdoor");
        Handle transferred = sa.transferSkill(skill_handle, new_ctx);
        assert(transferred != Handle::UNDEFINED);
        assert(sa.getSkillCount() >= 2);
        std::cout << "   PASS" << std::endl;

        // 6. Get all skills
        std::cout << "\n6. Get all skills..." << std::endl;
        auto all = sa.getAllSkills();
        assert(all.size() == sa.getSkillCount());
        std::cout << "   PASS (" << all.size() << " skill(s))" << std::endl;

        // 7. Statistics
        std::cout << "\n7. Statistics..." << std::endl;
        auto stats = sa.getSkillStatistics();
        assert(stats.at("total_skills") >= 1.0);
        std::cout << "   PASS (total=" << stats.at("total_skills") << ")" << std::endl;

        std::cout << "\n=== All tests passed ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
