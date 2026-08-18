/*
 * standalone/tests/test_perception.cpp
 *
 * Tests for Phase 2 Perception classes:
 *   MultiModalSensor, PerceptualProcessor, AttentionManager, TextualSensor
 */

#include "test_runner.h"

#include <memory>
#include <string>
#include <vector>

#include "cog0/AttentionManager.h"
#include "cog0/MultiModalSensor.h"
#include "cog0/PerceptualProcessor.h"
#include "cog0/TextualSensor.h"

using namespace cog0;

// -----------------------------------------------------------------------
// MultiModalSensor tests

TEST(multimodal_sensor_default_modality_is_text) {
    MultiModalSensor sensor("test-sensor");
    ASSERT_EQ(sensor.modality(), MultiModalSensor::Modality::TEXT);
    ASSERT_EQ(sensor.modalityName(sensor.modality()), std::string("text"));
}

TEST(multimodal_sensor_numeric_modality) {
    MultiModalSensor sensor("num-sensor", MultiModalSensor::Modality::NUMERIC);
    ASSERT_EQ(sensor.modality(), MultiModalSensor::Modality::NUMERIC);
    ASSERT_EQ(sensor.modalityName(sensor.modality()), std::string("numeric"));
}

TEST(multimodal_sensor_event_modality) {
    MultiModalSensor sensor("evt-sensor", MultiModalSensor::Modality::EVENT);
    ASSERT_EQ(sensor.modality(), MultiModalSensor::Modality::EVENT);
    ASSERT_EQ(sensor.modalityName(sensor.modality()), std::string("event"));
}

TEST(multimodal_sensor_visual_modality) {
    MultiModalSensor sensor("vis-sensor", MultiModalSensor::Modality::VISUAL);
    ASSERT_EQ(sensor.modality(), MultiModalSensor::Modality::VISUAL);
    ASSERT_EQ(sensor.modalityName(sensor.modality()), std::string("visual"));

    std::vector<PerceptInput> received;
    sensor.registerCallback([&](const PerceptInput& p) { received.push_back(p); });
    sensor.ingestVisual({0.1, 0.5, 0.9}, 0.75);
    ASSERT_EQ(received.size(), 1u);
    ASSERT_EQ(received[0].modality, std::string("visual"));
    ASSERT_EQ(received[0].salience, 0.75);
    ASSERT_FALSE(received[0].content.empty());
}

TEST(multimodal_sensor_ingest_fires_callback) {
    MultiModalSensor sensor("s1");
    std::vector<PerceptInput> received;
    sensor.registerCallback([&](const PerceptInput& p) { received.push_back(p); });

    sensor.ingest("hello world", 0.8);

    ASSERT_EQ(received.size(), 1u);
    ASSERT_EQ(received[0].content, std::string("hello world"));
    ASSERT_EQ(received[0].source, std::string("s1"));
    ASSERT_EQ(received[0].salience, 0.8);
    ASSERT_EQ(received[0].modality, std::string("text"));
}

TEST(multimodal_sensor_ingest_count) {
    MultiModalSensor sensor("s2");
    ASSERT_EQ(sensor.ingestCount(), 0u);
    sensor.ingest("a");
    sensor.ingest("b");
    ASSERT_EQ(sensor.ingestCount(), 2u);
}

TEST(multimodal_sensor_inactive_does_not_fire_callback) {
    MultiModalSensor sensor("s3");
    int callCount = 0;
    sensor.registerCallback([&](const PerceptInput&) { ++callCount; });

    sensor.setActive(false);
    sensor.ingest("should be suppressed");

    ASSERT_EQ(callCount, 0);
    ASSERT_EQ(sensor.ingestCount(), 0u);
}

TEST(multimodal_sensor_clear_callbacks) {
    MultiModalSensor sensor("s4");
    int callCount = 0;
    sensor.registerCallback([&](const PerceptInput&) { ++callCount; });
    sensor.clearCallbacks();
    sensor.ingest("no callback");
    ASSERT_EQ(callCount, 0);
}

TEST(multimodal_sensor_ingest_numeric) {
    MultiModalSensor sensor("num", MultiModalSensor::Modality::NUMERIC);
    std::vector<PerceptInput> received;
    sensor.registerCallback([&](const PerceptInput& p) { received.push_back(p); });

    sensor.ingestNumeric(3.14, 0.6);

    ASSERT_EQ(received.size(), 1u);
    ASSERT_EQ(received[0].modality, std::string("numeric"));
    ASSERT_FALSE(received[0].content.empty());
    ASSERT_EQ(received[0].salience, 0.6);
}

TEST(multimodal_sensor_ingest_event) {
    MultiModalSensor sensor("evt", MultiModalSensor::Modality::EVENT);
    std::vector<PerceptInput> received;
    sensor.registerCallback([&](const PerceptInput& p) { received.push_back(p); });

    sensor.ingestEvent("collision-detected", 0.9);

    ASSERT_EQ(received.size(), 1u);
    ASSERT_EQ(received[0].content, std::string("collision-detected"));
}

TEST(multimodal_sensor_multiple_callbacks) {
    MultiModalSensor sensor("multi");
    int count1 = 0, count2 = 0;
    sensor.registerCallback([&](const PerceptInput&) { ++count1; });
    sensor.registerCallback([&](const PerceptInput&) { ++count2; });
    sensor.ingest("ping");
    ASSERT_EQ(count1, 1);
    ASSERT_EQ(count2, 1);
}

// -----------------------------------------------------------------------
// PerceptualProcessor tests

TEST(perceptual_processor_process_text_creates_atoms) {
    auto store = std::make_shared<AtomStore>();
    PerceptualProcessor proc(store);

    auto handles = proc.processText("hello world", 0.7);

    ASSERT_GE(handles.size(), 1u);
    ASSERT_EQ(store->size(), handles.size());
}

TEST(perceptual_processor_text_salience_applied) {
    auto store = std::make_shared<AtomStore>();
    PerceptualProcessor proc(store);

    auto handles = proc.processText("test salience", 0.9);

    for (auto& h : handles)
        ASSERT_EQ(h->sti(), 0.9);
}

TEST(perceptual_processor_process_numeric) {
    auto store = std::make_shared<AtomStore>();
    PerceptualProcessor proc(store);

    auto h = proc.processNumeric(42.0, 0.6);

    ASSERT_TRUE(h != nullptr);
    ASSERT_EQ(h->type(), AtomType::CONCEPT);
    ASSERT_EQ(h->sti(), 0.6);
    // Name should contain "numeric:"
    ASSERT_FALSE(h->name().find("numeric:") == std::string::npos);
}

TEST(perceptual_processor_process_event) {
    auto store = std::make_shared<AtomStore>();
    PerceptualProcessor proc(store);

    auto h = proc.processEvent("sensor-triggered", 0.8);

    ASSERT_TRUE(h != nullptr);
    ASSERT_EQ(h->type(), AtomType::CONCEPT);
    // Name should contain "event:"
    ASSERT_FALSE(h->name().find("event:") == std::string::npos);
}

TEST(perceptual_processor_recent_percepts) {
    auto store = std::make_shared<AtomStore>();
    PerceptualProcessor proc(store);

    ASSERT_EQ(proc.recentPercepts().size(), 0u);
    proc.processText("alpha beta", 0.5);
    ASSERT_GE(proc.recentPercepts().size(), 1u);
}

TEST(perceptual_processor_clear_history) {
    auto store = std::make_shared<AtomStore>();
    PerceptualProcessor proc(store);

    proc.processText("one two three", 0.5);
    ASSERT_GT(proc.processedCount(), 0u);

    proc.clearHistory();
    ASSERT_EQ(proc.processedCount(), 0u);
    // Atoms should still be in the store
    ASSERT_GT(store->size(), 0u);
}

TEST(perceptual_processor_process_percept_input_text) {
    auto store = std::make_shared<AtomStore>();
    PerceptualProcessor proc(store);

    PerceptInput p;
    p.source   = "stdin";
    p.modality = "text";
    p.content  = "the quick brown fox";
    p.salience = 0.6;

    auto h = proc.process(p);
    ASSERT_TRUE(h != nullptr);
    ASSERT_GT(store->size(), 0u);
}

TEST(perceptual_processor_process_percept_input_event) {
    auto store = std::make_shared<AtomStore>();
    PerceptualProcessor proc(store);

    PerceptInput p;
    p.source   = "sensor";
    p.modality = "event";
    p.content  = "door-opened";
    p.salience = 0.75;

    auto h = proc.process(p);
    ASSERT_TRUE(h != nullptr);
    ASSERT_EQ(h->sti(), 0.75);
}

TEST(perceptual_processor_duplicate_tokens_reuse_atoms) {
    auto store = std::make_shared<AtomStore>();
    PerceptualProcessor proc(store);

    proc.processText("cat cat cat", 0.5);
    // All three tokens normalize to "cat" — AtomStore deduplicates
    auto h = store->getNode(AtomType::CONCEPT, "cat");
    ASSERT_TRUE(h != nullptr);
}

// -----------------------------------------------------------------------
// AttentionManager tests

TEST(attention_manager_stimulate_increases_sti) {
    auto store = std::make_shared<AtomStore>();
    auto h = store->addNode(AtomType::CONCEPT, "important");
    h->setSTI(0.0);

    AttentionManager attn(store);
    attn.stimulate(h, 0.3);

    ASSERT_EQ(h->sti(), 0.3);
}

TEST(attention_manager_decay_reduces_sti) {
    auto store = std::make_shared<AtomStore>();
    auto h = store->addNode(AtomType::CONCEPT, "fading");
    h->setSTI(1.0);

    AttentionManager attn(store);
    attn.decay(0.5);

    ASSERT_EQ(h->sti(), 0.5);
}

TEST(attention_manager_top_n) {
    auto store = std::make_shared<AtomStore>();
    auto h1 = store->addNode(AtomType::CONCEPT, "high");
    auto h2 = store->addNode(AtomType::CONCEPT, "mid");
    auto h3 = store->addNode(AtomType::CONCEPT, "low");
    h1->setSTI(0.9);
    h2->setSTI(0.5);
    h3->setSTI(0.1);

    AttentionManager attn(store);
    auto top2 = attn.topN(2);

    ASSERT_EQ(top2.size(), 2u);
    ASSERT_EQ(top2[0], h1);
    ASSERT_EQ(top2[1], h2);
}

TEST(attention_manager_above_threshold) {
    auto store = std::make_shared<AtomStore>();
    auto h1 = store->addNode(AtomType::CONCEPT, "a");
    auto h2 = store->addNode(AtomType::CONCEPT, "b");
    auto h3 = store->addNode(AtomType::CONCEPT, "c");
    h1->setSTI(0.8);
    h2->setSTI(0.3);
    h3->setSTI(0.6);

    AttentionManager attn(store);
    auto above = attn.aboveThreshold(0.5);

    ASSERT_EQ(above.size(), 2u);
}

TEST(attention_manager_focus_set_limited_size) {
    auto store = std::make_shared<AtomStore>();
    for (int i = 0; i < 10; ++i) {
        auto h = store->addNode(AtomType::CONCEPT, "atom-" + std::to_string(i));
        h->setSTI(static_cast<double>(i) * 0.1);
    }

    AttentionManager attn(store);
    auto fs = attn.focusSet(5);

    ASSERT_EQ(fs.size(), 5u);
    // Should be sorted descending by STI
    for (size_t i = 1; i < fs.size(); ++i)
        ASSERT_GE(fs[i-1]->sti(), fs[i]->sti());
}

TEST(attention_manager_normalize) {
    auto store = std::make_shared<AtomStore>();
    auto h1 = store->addNode(AtomType::CONCEPT, "x");
    auto h2 = store->addNode(AtomType::CONCEPT, "y");
    h1->setSTI(1.0);
    h2->setSTI(3.0);

    AttentionManager attn(store);
    attn.normalize(1.0); // mean should become 1.0

    double mean = (h1->sti() + h2->sti()) / 2.0;
    // mean should be close to 1.0
    ASSERT_GT(mean, 0.9);
    ASSERT_LT(mean, 1.1);
}

TEST(attention_manager_clamp) {
    auto store = std::make_shared<AtomStore>();
    auto h1 = store->addNode(AtomType::CONCEPT, "p");
    auto h2 = store->addNode(AtomType::CONCEPT, "q");
    h1->setSTI(-0.5);
    h2->setSTI(2.0);

    AttentionManager attn(store);
    attn.clamp(0.0, 1.0);

    ASSERT_EQ(h1->sti(), 0.0);
    ASSERT_EQ(h2->sti(), 1.0);
}

TEST(attention_manager_stimulate_null_handle_no_crash) {
    auto store = std::make_shared<AtomStore>();
    AttentionManager attn(store);
    // Should not throw or crash
    attn.stimulate(nullptr, 0.5);
}

// -----------------------------------------------------------------------
// TextualSensor tests

TEST(textual_sensor_default_name) {
    TextualSensor ts;
    ASSERT_EQ(ts.name(), std::string("textual-sensor"));
}

TEST(textual_sensor_custom_name) {
    TextualSensor ts("my-sensor");
    ASSERT_EQ(ts.name(), std::string("my-sensor"));
}

TEST(textual_sensor_ingest_returns_percept) {
    TextualSensor ts("ts1");
    auto p = ts.ingest("Hello from the sensor");

    ASSERT_EQ(p.source, std::string("ts1"));
    ASSERT_EQ(p.modality, std::string("text"));
    ASSERT_EQ(p.content, std::string("Hello from the sensor"));
    ASSERT_GT(p.salience, 0.0);
    ASSERT_LE(p.salience, 1.0);
}

TEST(textual_sensor_processed_count) {
    TextualSensor ts;
    ASSERT_EQ(ts.processedCount(), 0u);
    ts.ingest("one");
    ts.ingest("two");
    ts.ingest("three");
    ASSERT_EQ(ts.processedCount(), 3u);
}

TEST(textual_sensor_ingest_with_salience) {
    TextualSensor ts;
    auto p = ts.ingestWithSalience("custom salience", 0.77);
    ASSERT_EQ(p.salience, 0.77);
}

TEST(textual_sensor_empty_text_low_salience) {
    TextualSensor ts;
    double s = ts.computeSalience("");
    ASSERT_EQ(s, 0.0);
}

TEST(textual_sensor_keyword_boosts_salience) {
    TextualSensor ts;
    double withKw    = ts.computeSalience("This is a critical goal");
    double withoutKw = ts.computeSalience("abcdefghijkl");
    // keyword presence should yield higher salience
    ASSERT_GE(withKw, withoutKw);
}

TEST(textual_sensor_salience_in_range) {
    TextualSensor ts;
    std::vector<std::string> samples = {
        "", "x", "short text",
        "A moderately long text that discusses several important goals and tasks.",
        "critical urgent alert error warning primary key main"
    };
    for (const auto& s : samples) {
        double sal = ts.computeSalience(s);
        ASSERT_GE(sal, 0.0);
        ASSERT_LE(sal, 1.0);
    }
}

// -----------------------------------------------------------------------
// Integration: sensor → processor → attention manager

TEST(perception_pipeline_sensor_processor_attention) {
    auto store = std::make_shared<AtomStore>();
    PerceptualProcessor proc(store);
    AttentionManager    attn(store);
    TextualSensor       ts("pipeline-sensor");

    // Ingest text through the sensor
    auto p = ts.ingest("the agent has reached its primary goal");

    // Process through the perceptual processor
    proc.process(p);

    // Stimulate atoms above a threshold (simulate attention update)
    auto recent = proc.recentPercepts();
    for (auto& h : recent)
        attn.stimulate(h, 0.2);

    // Focus set should contain some atoms
    auto focus = attn.focusSet(10);
    ASSERT_GT(focus.size(), 0u);

    // All focus set atoms should have STI > 0
    for (auto& h : focus)
        ASSERT_GT(h->sti(), 0.0);
}
