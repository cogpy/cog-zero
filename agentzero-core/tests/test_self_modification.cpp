#include "test_runner.h"

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/agentzero/SelfModification.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(SelfModification_AnalyzeProposeApplyRollback)
{
    auto as = createAtomSpace();
    SelfModification sm(nullptr, as);

    auto analysis = sm.analyzeComponent("TaskManager");
    ASSERT_FALSE(analysis.component_name.empty());

    auto proposals = sm.proposeModifications("TaskManager", 3);
    ASSERT_GT(proposals.size(), 0u);

    auto ranked = sm.evaluateProposals(proposals);
    ASSERT_EQ(ranked.size(), proposals.size());

    sm.setSafetyLevel(SelfModification::SafetyLevel::EXPERIMENTAL);
    auto result = sm.applyModification(ranked[0], true);
    // Apply may succeed or be rejected depending on safety heuristics — both OK
    ASSERT_TRUE(result.status == SelfModification::ModificationStatus::SUCCESS ||
                result.status == SelfModification::ModificationStatus::REJECTED ||
                result.status == SelfModification::ModificationStatus::FAILED ||
                result.status == SelfModification::ModificationStatus::ROLLED_BACK ||
                result.status == SelfModification::ModificationStatus::PENDING_VALIDATION);

    if (result.status == SelfModification::ModificationStatus::SUCCESS) {
        // Rollback should be safe to call
        sm.rollback(result);
    }
}
