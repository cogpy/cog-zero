#include "test_runner.h"

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/agentzero/AttentionManager.h>
#include <opencog/agentzero/MultiModalSensor.h>
#include <opencog/agentzero/PerceptualProcessor.h>
#include <opencog/agentzero/TextualSensor.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(Pipeline_SensorToProcessorToAttention)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "PipelineAgent");

    PerceptualProcessor processor(as, self);
    AttentionManager attention(as);

    SensorInfo info("PipelineCam", "pipeline camera", SensorCapability::VISUAL, 10.0);
    MockSensor sensor(info);

    std::vector<Handle> encoded;
    sensor.registerCallback([&](const SensoryInput& input) {
        Handle h = processor.processInput(input);
        if (h) {
            SalienceScore score = attention.calculateSalience(input);
            attention.allocateAttention(h, score.overall);
            encoded.push_back(h);
        }
    });

    sensor.initialize();
    sensor.start();
    sensor.addTestData({0.1, 0.5, 0.9, 0.2});
    sensor.addTestData({0.8, 0.3, 0.4});
    ASSERT_TRUE(sensor.generateNextSample());
    ASSERT_TRUE(sensor.generateNextSample());

    ASSERT_EQ(encoded.size(), 2u);
    ASSERT_EQ(attention.trackedAtomCount(), 2u);
    ASSERT_GT(as->get_handles_by_type(EVALUATION_LINK).size(), 0u);

    for (const auto& h : encoded) {
        ASSERT_TRUE(as->is_valid_handle(h));
        ASSERT_TRUE(attention.getSTI(h) > 0.0);
    }
}

TEST(Pipeline_TextualThroughAttention)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TextAgent");
    TextualSensor text(as, self);
    AttentionManager attention(as);
    PerceptualProcessor processor(as, self);

    text.add_text("Critical goal detected. Proceed with urgent task.");
    auto units = text.processAll();
    ASSERT_FALSE(units.empty());

    // Also route a SensoryInput conversion through processor
    auto score = text.calculateSalience("critical alert");
    SensoryInput si = text.toSensoryInput("critical alert", score);
    Handle percept = processor.processInput(si);
    ASSERT_NE(percept, Handle::UNDEFINED);

    double sti = attention.allocateAttention(percept, score.overall);
    ASSERT_TRUE(sti > 0.0);
    ASSERT_TRUE(attention.trackedAtomCount() >= 1u);
}

TEST(Pipeline_VisualModalitySupported)
{
    // ROADMAP: MultiModalSensor covers text, numeric, event, and visual inputs
    SensorInfo visual("V", "v", SensorCapability::VISUAL, 30.0);
    SensorInfo numeric("N", "n", SensorCapability::NUMERIC, 1.0);
    SensorInfo event("E", "e", SensorCapability::EVENT, 1.0);
    SensorInfo textual("T", "t", SensorCapability::TEXTUAL, 1.0);

    ASSERT_EQ(MultiModalSensor::capabilityToType(visual.capabilities),
              std::string("visual"));
    ASSERT_EQ(MultiModalSensor::capabilityToType(numeric.capabilities),
              std::string("numeric"));
    ASSERT_EQ(MultiModalSensor::capabilityToType(event.capabilities),
              std::string("event"));
    ASSERT_EQ(MultiModalSensor::capabilityToType(textual.capabilities),
              std::string("textual"));
}
