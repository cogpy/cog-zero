#include "test_runner.h"

#include <opencog/agentzero/MultiModalSensor.h>

using namespace opencog::agentzero;

TEST(MultiModal_ConstructorAndCapabilities)
{
    SensorInfo info("TestSensor", "Test sensor for unit testing",
                    SensorCapability::VISUAL, 10.0);
    MockSensor sensor(info);

    ASSERT_EQ(sensor.getSensorInfo().name, std::string("TestSensor"));
    ASSERT_EQ(sensor.getSensorInfo().description, std::string("Test sensor for unit testing"));
    ASSERT_EQ(sensor.getSensorInfo().sampling_rate, 10.0);
    ASSERT_FALSE(sensor.isActive());
    ASSERT_TRUE(sensor.hasCapability(SensorCapability::VISUAL));
    ASSERT_FALSE(sensor.hasCapability(SensorCapability::AUDITORY));
}

TEST(MultiModal_InitializeStartStop)
{
    SensorInfo info("Cam", "camera", SensorCapability::VISUAL, 30.0);
    MockSensor sensor(info);

    ASSERT_FALSE(sensor.isInitialized());
    ASSERT_FALSE(sensor.start()); // cannot start before init
    ASSERT_TRUE(sensor.initialize());
    ASSERT_TRUE(sensor.isInitialized());
    ASSERT_TRUE(sensor.start());
    ASSERT_TRUE(sensor.isActive());
    ASSERT_TRUE(sensor.stop());
    ASSERT_FALSE(sensor.isActive());
}

TEST(MultiModal_MultiCapabilities)
{
    SensorCapability multi = SensorCapability::VISUAL | SensorCapability::AUDITORY;
    SensorInfo info("Multi", "multi", multi, 20.0);
    MockSensor sensor(info);
    ASSERT_TRUE(sensor.hasCapability(SensorCapability::VISUAL));
    ASSERT_TRUE(sensor.hasCapability(SensorCapability::AUDITORY));
    ASSERT_FALSE(sensor.hasCapability(SensorCapability::TACTILE));
}

TEST(MultiModal_CallbacksAndGenerate)
{
    SensorInfo info("TestSensor", "test", SensorCapability::VISUAL, 10.0);
    MockSensor sensor(info);

    bool called = false;
    SensoryInput received;
    sensor.registerCallback([&](const SensoryInput& in) {
        called = true;
        received = in;
    });

    sensor.initialize();
    sensor.start();

    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
    sensor.addTestData(data);
    ASSERT_TRUE(sensor.generateNextSample());
    ASSERT_TRUE(called);
    ASSERT_EQ(received.data, data);
    ASSERT_EQ(received.sensor_type, std::string("visual"));
}

TEST(MultiModal_MultipleCallbacks)
{
    SensorInfo info("S", "s", SensorCapability::VISUAL, 1.0);
    MockSensor sensor(info);
    int count = 0;
    for (int i = 0; i < 3; ++i) {
        sensor.registerCallback([&](const SensoryInput&) { ++count; });
    }
    sensor.initialize();
    sensor.start();
    sensor.generateNextSample();
    ASSERT_EQ(count, 3);
}

TEST(MultiModal_ClearCallbacks)
{
    SensorInfo info("S", "s", SensorCapability::VISUAL, 1.0);
    MockSensor sensor(info);
    bool called = false;
    sensor.registerCallback([&](const SensoryInput&) { called = true; });
    sensor.clearCallbacks();
    sensor.initialize();
    sensor.start();
    sensor.generateNextSample();
    ASSERT_FALSE(called);
}

TEST(MultiModal_GenerateWithoutStart)
{
    SensorInfo info("S", "s", SensorCapability::VISUAL, 1.0);
    MockSensor sensor(info);
    sensor.initialize();
    ASSERT_FALSE(sensor.generateNextSample());
}

TEST(MultiModal_StatusInfo)
{
    SensorInfo info("TestSensor", "test", SensorCapability::VISUAL, 10.0);
    MockSensor sensor(info);
    std::string st = sensor.getStatusInfo();
    ASSERT_TRUE(st.find("\"name\":\"TestSensor\"") != std::string::npos);
    ASSERT_TRUE(st.find("\"is_active\":false") != std::string::npos);
    ASSERT_TRUE(st.find("\"is_initialized\":false") != std::string::npos);

    sensor.initialize();
    sensor.start();
    st = sensor.getStatusInfo();
    ASSERT_TRUE(st.find("\"is_active\":true") != std::string::npos);
    ASSERT_TRUE(st.find("\"is_initialized\":true") != std::string::npos);
}

TEST(MultiModal_SensorTypeMapping)
{
    {
        SensorInfo info("A", "a", SensorCapability::AUDITORY, 44100.0);
        MockSensor sensor(info);
        std::string type;
        sensor.registerCallback([&](const SensoryInput& in) { type = in.sensor_type; });
        sensor.initialize();
        sensor.start();
        sensor.generateNextSample();
        ASSERT_EQ(type, std::string("auditory"));
    }
    {
        SensorInfo info("T", "t", SensorCapability::TACTILE, 1000.0);
        MockSensor sensor(info);
        std::string type;
        sensor.registerCallback([&](const SensoryInput& in) { type = in.sensor_type; });
        sensor.initialize();
        sensor.start();
        sensor.generateNextSample();
        ASSERT_EQ(type, std::string("tactile"));
    }
}

TEST(MultiModal_CallbackExceptionIsolated)
{
    SensorInfo info("S", "s", SensorCapability::VISUAL, 1.0);
    MockSensor sensor(info);
    sensor.registerCallback([](const SensoryInput&) {
        throw std::runtime_error("Test exception");
    });
    bool normal = false;
    sensor.registerCallback([&](const SensoryInput&) { normal = true; });
    sensor.initialize();
    sensor.start();
    ASSERT_TRUE(sensor.generateNextSample());
    ASSERT_TRUE(normal);
}

TEST(MultiModal_TestDataCycles)
{
    SensorInfo info("S", "s", SensorCapability::VISUAL, 1.0);
    MockSensor sensor(info);
    std::vector<double> d1 = {1.0, 2.0, 3.0};
    std::vector<double> d2 = {4.0, 5.0, 6.0};
    sensor.addTestData(d1);
    sensor.addTestData(d2);

    std::vector<std::vector<double>> received;
    sensor.registerCallback([&](const SensoryInput& in) {
        received.push_back(in.data);
    });
    sensor.initialize();
    sensor.start();
    sensor.generateNextSample();
    sensor.generateNextSample();
    sensor.generateNextSample(); // cycles
    ASSERT_EQ(received.size(), 3u);
    ASSERT_EQ(received[0], d1);
    ASSERT_EQ(received[1], d2);
    ASSERT_EQ(received[2], d1);
}
