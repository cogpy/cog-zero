#include "test_runner.h"

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/agentzero/KnowledgeIntegrator.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(KnowledgeIntegrator_AddSearchMine)
{
    auto as = createAtomSpace();
    KnowledgeIntegrator ki(nullptr, as);

    Handle k1 = ki.addKnowledge("gravity", "objects attract", KnowledgeIntegrator::KnowledgeType::FACTUAL);
    Handle k2 = ki.addFact("Earth", "orbits", "Sun");
    ASSERT_NE(k1, Handle::UNDEFINED);
    ASSERT_NE(k2, Handle::UNDEFINED);
    ASSERT_EQ(ki.knowledgeCount(), 2u);

    auto hits = ki.semanticSearch("Earth");
    ASSERT_GT(hits.size(), 0u);

    auto patterns = ki.minePatterns(1);
    ASSERT_GT(patterns.size(), 0u);

    ASSERT_TRUE(ki.processKnowledgeIntegration());
    ASSERT_NE(ki.getKnowledgeBaseAtom(), Handle::UNDEFINED);
}
