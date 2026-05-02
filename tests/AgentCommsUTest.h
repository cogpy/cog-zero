/*
 * AgentCommsUTest.h
 *
 * Unit tests for AgentComms class
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#include <cxxtest/TestSuite.h>
#include <memory>
#include <chrono>
#include <thread>

#include <opencog/agentzero/communication/AgentComms.h>
#include <opencog/agentzero/communication/CommTypes.h>
#include <opencog/atomspace/AtomSpace.h>

using namespace opencog;
using namespace opencog::agentzero::communication;

class AgentCommsUTest : public CxxTest::TestSuite {
private:
    AtomSpacePtr atomspace;
    std::unique_ptr<AgentComms> comms;
    
public:
    void setUp() {
        atomspace = createAtomSpace();
        
        AgentId test_agent("TestAgent", "instance1");
        CommConfig config;
        config.enable_network = false;
        config.enable_persistence = true;
        
        comms = std::make_unique<AgentComms>(test_agent, config, atomspace);
    }
    
    void tearDown() {
        if (comms) {
            comms->stop();
            comms.reset();
        }
        atomspace.reset();
    }
    
    void testInitialization() {
        TS_ASSERT(comms != nullptr);
        TS_ASSERT(comms->isInitialized());
        TS_ASSERT_EQUALS(comms->getAgentId().name, "TestAgent");
        TS_ASSERT_EQUALS(comms->getAgentId().instance_id, "instance1");
        TS_ASSERT(comms->getAtomSpace() == atomspace);
    }
    
    void testStartStop() {
        TS_ASSERT(!comms->isRunning());
        
        bool started = comms->start();
        TS_ASSERT(started);
        TS_ASSERT(comms->isRunning());
        
        bool stopped = comms->stop();
        TS_ASSERT(stopped);
        TS_ASSERT(!comms->isRunning());
    }
    
    void testSendMessage() {
        TS_ASSERT(comms->start());
        
        AgentId recipient("RecipientAgent");
        std::string content = "Hello, World!";
        
        std::string message_id = comms->sendMessage(
            recipient,
            MessageType::INFO,
            content,
            MessagePriority::NORMAL,
            ProtocolType::LOCAL
        );
        
        TS_ASSERT(!message_id.empty());
        TS_ASSERT(message_id.find("msg_") == 0);  // Should start with "msg_"
        
        comms->stop();
    }
    
    void testSendAtomMessage() {
        TS_ASSERT(comms->start());
        
        // Create a test atom
        Handle test_atom = atomspace->add_node(CONCEPT_NODE, "TestConcept");
        TS_ASSERT(test_atom != Handle::UNDEFINED);
        
        AgentId recipient("RecipientAgent");
        
        std::string message_id = comms->sendAtomMessage(
            recipient,
            MessageType::KNOWLEDGE_SHARE,
            test_atom,
            MessagePriority::HIGH,
            ProtocolType::LOCAL
        );
        
        TS_ASSERT(!message_id.empty());
        
        comms->stop();
    }
    
    void testMessageHandler() {
        TS_ASSERT(comms->start());
        
        bool handler_called = false;
        std::string received_content;
        
        // Register message handler
        MessageHandler handler = [&](const CommMessagePtr& msg) -> bool {
            handler_called = true;
            received_content = msg->content;
            return true;
        };
        
        bool registered = comms->registerMessageHandler(MessageType::INFO, handler);
        TS_ASSERT(registered);
        
        // Create and send a message (for testing, we'd need a way to inject it into incoming queue)
        // This is a limitation of the current test - in a full implementation, 
        // we'd have a way to simulate receiving messages
        
        // Unregister handler
        bool unregistered = comms->unregisterMessageHandler(MessageType::INFO);
        TS_ASSERT(unregistered);
        
        comms->stop();
    }
    
    void testBroadcastMessage() {
        TS_ASSERT(comms->start());
        
        std::vector<AgentId> recipients = {
            AgentId("Agent1"),
            AgentId("Agent2"),
            AgentId("Agent3")
        };
        
        size_t success_count = comms->broadcastMessage(
            recipients,
            MessageType::NOTIFICATION,
            "Broadcast message",
            MessagePriority::NORMAL
        );
        
        TS_ASSERT_EQUALS(success_count, recipients.size());
        
        comms->stop();
    }
    
    void testConfiguration() {
        CommConfig original_config = comms->getConfig();
        
        CommConfig new_config = original_config;
        new_config.max_message_size = 2048;
        new_config.message_timeout = std::chrono::seconds(60);
        
        bool updated = comms->updateConfig(new_config);
        TS_ASSERT(updated);
        
        CommConfig current_config = comms->getConfig();
        TS_ASSERT_EQUALS(current_config.max_message_size, 2048u);
        TS_ASSERT_EQUALS(current_config.message_timeout.count(), 60);
    }
    
    void testStatistics() {
        TS_ASSERT(comms->start());
        
        // Initial stats should be zero
        CommStats stats = comms->getStats();
        TS_ASSERT_EQUALS(stats.messages_sent, 0u);
        TS_ASSERT_EQUALS(stats.messages_received, 0u);
        TS_ASSERT_EQUALS(stats.messages_failed, 0u);
        
        // Send a message
        AgentId recipient("TestRecipient");
        comms->sendMessage(recipient, MessageType::INFO, "test", MessagePriority::NORMAL, ProtocolType::LOCAL);
        
        // Stats should be updated (in a real implementation)
        // Note: This test may need adjustment based on actual stats implementation
        
        // Reset stats
        comms->resetStats();
        stats = comms->getStats();
        TS_ASSERT_EQUALS(stats.messages_sent, 0u);
        
        comms->stop();
    }
    
    void testStatusInfo() {
        std::string status = comms->getStatusInfo();
        TS_ASSERT(!status.empty());
        TS_ASSERT(status.find("TestAgent") != std::string::npos);
        TS_ASSERT(status.find("initialized") != std::string::npos);
    }
    
    void testPendingMessageCount() {
        size_t pending = comms->getPendingMessageCount();
        TS_ASSERT_EQUALS(pending, 0u);  // Should be zero initially
    }
};