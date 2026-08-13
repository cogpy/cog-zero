#include "test_runner.h"

#include <stdexcept>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/agentzero/TextualSensor.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(Textual_ConstructorDefault)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    TextualSensor sensor(as, self);
    ASSERT_EQ(static_cast<int>(sensor.getMode()),
              static_cast<int>(TextProcessingMode::SENTENCES));
    ASSERT_EQ(sensor.queue_size(), 0u);
    ASSERT_EQ(sensor.processedUnitCount(), 0u);
}

TEST(Textual_ConstructorWordsMode)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    TextualSensor ws(as, self, TextProcessingMode::WORDS);
    ASSERT_EQ(static_cast<int>(ws.getMode()),
              static_cast<int>(TextProcessingMode::WORDS));
}

TEST(Textual_NullArgsThrow)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    bool threw1 = false, threw2 = false;
    try { TextualSensor bad(nullptr, self); } catch (const std::invalid_argument&) { threw1 = true; }
    try { TextualSensor bad(as, Handle::UNDEFINED); } catch (const std::invalid_argument&) { threw2 = true; }
    ASSERT_TRUE(threw1);
    ASSERT_TRUE(threw2);
}

TEST(Textual_QueueManagement)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    TextualSensor sensor(as, self);

    ASSERT_EQ(sensor.queue_size(), 0u);
    sensor.add_text("Hello world.");
    ASSERT_EQ(sensor.queue_size(), 1u);
    sensor.add_text("Another sentence.");
    ASSERT_EQ(sensor.queue_size(), 2u);

    sensor.add_text("");
    ASSERT_EQ(sensor.queue_size(), 2u);

    sensor.processNext();
    ASSERT_EQ(sensor.queue_size(), 1u);
    sensor.processNext();
    ASSERT_EQ(sensor.queue_size(), 0u);

    auto empty = sensor.processNext();
    ASSERT_TRUE(empty.empty());

    sensor.add_text("Alpha.");
    sensor.add_text("Beta.");
    sensor.add_text("Gamma.");
    sensor.processAll();
    ASSERT_EQ(sensor.queue_size(), 0u);
}

TEST(Textual_ProcessingModes)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    TextualSensor sensor(as, self);

    auto sentences = sensor.processText("First sentence. Second sentence. Third!");
    ASSERT_TRUE(sentences.size() >= 2u);

    sensor.setMode(TextProcessingMode::WORDS);
    auto words = sensor.processText("hello world foo");
    ASSERT_EQ(words.size(), 3u);

    sensor.setMode(TextProcessingMode::DOCUMENTS);
    auto docs = sensor.processText("Full document. Multiple sentences! No split.");
    ASSERT_EQ(docs.size(), 1u);

    sensor.setMode(TextProcessingMode::STREAM);
    auto stream = sensor.processText("raw stream data");
    ASSERT_EQ(stream.size(), 1u);

    sensor.setMode(TextProcessingMode::DOCUMENTS);
    ASSERT_EQ(static_cast<int>(sensor.getMode()),
              static_cast<int>(TextProcessingMode::DOCUMENTS));
    sensor.setMode(TextProcessingMode::WORDS);
    ASSERT_EQ(static_cast<int>(sensor.getMode()),
              static_cast<int>(TextProcessingMode::WORDS));
}

TEST(Textual_AtomCreation)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    TextualSensor sensor(as, self);

    ASSERT_EQ(sensor.processedUnitCount(), 0u);
    auto handles = sensor.processText("The agent perceives the world.");
    ASSERT_FALSE(handles.empty());
    for (const auto& h : handles) {
        ASSERT_NE(h, Handle::UNDEFINED);
        ASSERT_TRUE(as->is_valid_handle(h));
    }
    ASSERT_TRUE(sensor.processedUnitCount() > 0u);
}

TEST(Textual_Salience)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    TextualSensor sensor(as, self);

    auto score = sensor.calculateSalience("Hello world");
    ASSERT_TRUE(score.lexical >= 0.0 && score.lexical <= 1.0);
    ASSERT_TRUE(score.length >= 0.0 && score.length <= 1.0);
    ASSERT_TRUE(score.novelty >= 0.0 && score.novelty <= 1.0);
    ASSERT_TRUE(score.overall >= 0.0 && score.overall <= 1.0);

    auto empty = sensor.calculateSalience("");
    ASSERT_NEAR(empty.overall, 0.0, 1e-9);

    auto first = sensor.calculateSalience("unique_word_xyz");
    auto second = sensor.calculateSalience("unique_word_xyz");
    ASSERT_TRUE(first.novelty >= second.novelty);

    auto short_s = sensor.calculateSalience("short");
    auto long_s = sensor.calculateSalience(
        "a much longer text with many more words to test the length scoring component");
    ASSERT_TRUE(long_s.length >= short_s.length);
}

TEST(Textual_ToSensoryInput)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    TextualSensor sensor(as, self);

    TextSalienceScore score(0.5, 0.5, 0.5, 0.5);
    SensoryInput si = sensor.toSensoryInput("hello", score);
    ASSERT_EQ(si.sensor_type, std::string("textual"));
    ASSERT_EQ(si.modality, std::string("text_stream"));
    ASSERT_FALSE(si.data.empty());
    ASSERT_NEAR(si.confidence, 0.5, 1e-9);

    TextSalienceScore score2(0.8, 0.6, 0.7, 0.7);
    SensoryInput si2 = sensor.toSensoryInput("hi", score2);
    ASSERT_EQ(si2.data.size(), 2u);
    for (double d : si2.data) {
        ASSERT_TRUE(d >= 0.0 && d <= 1.0);
    }
}

TEST(Textual_StatsAndVocabulary)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    TextualSensor sensor(as, self);

    std::string stats = sensor.getStats();
    ASSERT_TRUE(stats.find("processed_units") != std::string::npos);
    ASSERT_TRUE(stats.find("queued_count") != std::string::npos);
    ASSERT_TRUE(stats.find("vocabulary_size") != std::string::npos);

    sensor.processText("some repeated words repeated again");
    auto before = sensor.calculateSalience("repeated");
    sensor.resetVocabulary();
    auto after = sensor.calculateSalience("repeated");
    ASSERT_TRUE(after.novelty >= before.novelty);
}

TEST(Textual_ConcurrentStyleDrain)
{
    auto as = createAtomSpace();
    Handle self = as->add_node(CONCEPT_NODE, "TestAgent");
    TextualSensor sensor(as, self);
    for (int i = 0; i < 10; ++i) {
        sensor.add_text("Sentence number " + std::to_string(i) + ".");
    }
    auto handles = sensor.processAll();
    ASSERT_FALSE(handles.empty());
    ASSERT_EQ(sensor.queue_size(), 0u);
}
