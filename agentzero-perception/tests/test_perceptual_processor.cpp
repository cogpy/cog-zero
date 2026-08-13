#include "test_runner.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/agentzero/PerceptualProcessor.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(Perceptual_ConstructorValid)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    PerceptualProcessor proc(as, self);
    ASSERT_EQ(proc.getAtomSpace(), as);
    ASSERT_EQ(proc.getAgentSelf(), self);
}

TEST(Perceptual_ConstructorInvalidAtomSpace)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    bool threw = false;
    try {
        PerceptualProcessor bad(nullptr, self);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(Perceptual_ConstructorInvalidAgentSelf)
{
    auto as = createAtomSpace();
    bool threw = false;
    try {
        PerceptualProcessor bad(as, Handle::UNDEFINED);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(Perceptual_ProcessModalities)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    PerceptualProcessor proc(as, self);

    {
        SensoryInput in("visual", "camera", {0.1, 0.5, 0.8}, 0.9);
        Handle h = proc.processInput(in);
        ASSERT_NE(h, Handle::UNDEFINED);
        ASSERT_TRUE(h->is_node());
        ASSERT_GT(as->get_size(), 0u);
    }
    {
        SensoryInput in("auditory", "microphone", {0.2, 0.4}, 0.8);
        ASSERT_NE(proc.processInput(in), Handle::UNDEFINED);
    }
    {
        SensoryInput in("tactile", "pressure", {0.3, 0.7}, 0.7);
        ASSERT_NE(proc.processInput(in), Handle::UNDEFINED);
    }
    {
        SensoryInput in("temperature", "thermal", {1.0, 2.0, 3.0}, 0.6);
        ASSERT_NE(proc.processInput(in), Handle::UNDEFINED);
    }
    {
        SensoryInput in("visual", "camera", {}, 0.5);
        ASSERT_NE(proc.processInput(in), Handle::UNDEFINED);
    }
}

TEST(Perceptual_RejectsInvalidConfidenceAndData)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    PerceptualProcessor proc(as, self);

    SensoryInput high("visual", "camera", {1.0, 2.0}, 1.5);
    ASSERT_EQ(proc.processInput(high), Handle::UNDEFINED);

    SensoryInput low("visual", "camera", {1.0, 2.0}, -0.5);
    ASSERT_EQ(proc.processInput(low), Handle::UNDEFINED);

    SensoryInput nan_in("visual", "camera",
                        {1.0, std::numeric_limits<double>::quiet_NaN()}, 0.8);
    ASSERT_EQ(proc.processInput(nan_in), Handle::UNDEFINED);

    SensoryInput inf_in("visual", "camera",
                        {1.0, std::numeric_limits<double>::infinity()}, 0.8);
    ASSERT_EQ(proc.processInput(inf_in), Handle::UNDEFINED);
}

TEST(Perceptual_ProcessBatch)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    PerceptualProcessor proc(as, self);

    std::vector<SensoryInput> inputs;
    inputs.emplace_back("visual", "camera", std::vector<double>{1.0, 2.0}, 0.9);
    inputs.emplace_back("auditory", "microphone", std::vector<double>{3.0, 4.0}, 0.8);
    inputs.emplace_back("tactile", "pressure", std::vector<double>{5.0, 6.0}, 0.7);

    auto results = proc.processBatch(inputs);
    ASSERT_EQ(results.size(), inputs.size());
    for (const auto& h : results) {
        ASSERT_NE(h, Handle::UNDEFINED);
    }
}

TEST(Perceptual_ContextAndStats)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    PerceptualProcessor proc(as, self);

    std::string initial = proc.getProcessingStats();
    ASSERT_TRUE(initial.find("\"processed_count\":0") != std::string::npos);
    ASSERT_TRUE(proc.isHealthy());

    Handle ctx = as->add_node(CONCEPT_NODE, "TestContext");
    proc.setPerceptionContext(ctx);

    SensoryInput valid("visual", "camera", {1.0, 2.0, 3.0}, 0.8);
    ASSERT_NE(proc.processInput(valid), Handle::UNDEFINED);

    SensoryInput invalid("visual", "camera", {1.0}, 2.0);
    ASSERT_EQ(proc.processInput(invalid), Handle::UNDEFINED);

    std::string stats = proc.getProcessingStats();
    ASSERT_TRUE(stats.find("\"processed_count\":1") != std::string::npos);
    ASSERT_TRUE(stats.find("\"error_count\":1") != std::string::npos);
    ASSERT_TRUE(proc.isHealthy());

    for (int i = 0; i < 20; ++i) {
        proc.processInput(invalid);
    }
    ASSERT_FALSE(proc.isHealthy());

    proc.resetStats();
    stats = proc.getProcessingStats();
    ASSERT_TRUE(stats.find("\"processed_count\":0") != std::string::npos);
    ASSERT_TRUE(stats.find("\"error_count\":0") != std::string::npos);
    ASSERT_TRUE(proc.isHealthy());
}

TEST(Perceptual_CreatesEvaluationLinks)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    PerceptualProcessor proc(as, self);
    SensoryInput in("visual", "camera", {0.5}, 0.9);
    Handle h = proc.processInput(in);
    ASSERT_NE(h, Handle::UNDEFINED);
    ASSERT_TRUE(as->is_valid_handle(h));
    ASSERT_GT(as->get_handles_by_type(EVALUATION_LINK).size(), 0u);
}

TEST(Perceptual_DistinctVectorsGetDistinctAtoms)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    PerceptualProcessor proc(as, self);

    SensoryInput a("visual", "cam", {1.0, 2.0, 3.0}, 0.9);
    SensoryInput b("visual", "cam", {1.0, 9.0, 3.0}, 0.9);
    Handle ha = proc.processInput(a);
    Handle hb = proc.processInput(b);
    ASSERT_NE(ha, Handle::UNDEFINED);
    ASSERT_NE(hb, Handle::UNDEFINED);
    ASSERT_NE(ha, hb);
}
