# Agent-Zero Communication Module

Implementation of communication protocols for Agent-Zero (AZ-COMM-001).

## Overview

The Agent-Zero Communication module provides a comprehensive communication infrastructure for Agent-Zero cognitive architecture, enabling message passing between agents across different protocols and transport mechanisms.

## Features

- **Multi-Protocol Support**: Local, network, IPC, and broadcast communication
- **AtomSpace Integration**: Messages can be persisted and represented as atoms
- **Message Routing**: Intelligent routing based on agent locations and protocol availability
- **Serialization**: Support for text and AtomSpace content serialization
- **Statistics & Monitoring**: Comprehensive statistics and health monitoring
- **CogServer Integration**: Optional network communication via CogServer
- **Thread-Safe**: All operations are thread-safe for concurrent access

## Architecture

### Core Components

1. **AgentComms**: Main communication interface
   - Message sending and receiving
   - Protocol management
   - Handler registration
   - Statistics tracking

2. **MessageRouter**: Intelligent message routing
   - Agent discovery and registration
   - Route optimization and caching
   - Multi-protocol routing decisions

3. **ProtocolManager**: Protocol handling
   - Local, network, IPC, and broadcast protocols
   - Protocol health monitoring
   - Load balancing and failover

4. **MessageSerializer**: Message serialization
   - JSON and binary serialization
   - AtomSpace content handling
   - Compression and validation

### Message Types

- **INFO**: Informational messages
- **REQUEST**: Action/information requests
- **RESPONSE**: Responses to requests
- **NOTIFICATION**: Event notifications
- **GOAL_UPDATE**: Goal state changes
- **TASK_ASSIGNMENT**: Task delegation
- **STATUS_REPORT**: Agent status updates
- **KNOWLEDGE_SHARE**: Knowledge/facts sharing
- **QUERY**: Knowledge queries
- **LEARNING_UPDATE**: Learning/adaptation updates
- **HEARTBEAT**: Keep-alive messages
- **ERROR**: Error reports
- **SHUTDOWN**: Shutdown notifications

### Communication Protocols

- **LOCAL**: In-process communication via AtomSpace
- **NETWORK**: Network communication via CogServer (optional)
- **IPC**: Inter-process communication
- **BROADCAST**: Multi-recipient messaging

## Usage

### Basic Usage

```cpp
#include <opencog/agentzero/communication/AgentComms.h>

// Create agent communication instance
AgentId my_agent("MyAgent", "instance1");
CommConfig config;
config.enable_persistence = true;

auto comms = std::make_unique<AgentComms>(my_agent, config);

// Start communication system
comms->start();

// Send a message
AgentId recipient("OtherAgent");
std::string msg_id = comms->sendMessage(
    recipient,
    MessageType::INFO,
    "Hello, World!",
    MessagePriority::NORMAL,
    ProtocolType::LOCAL
);

// Register message handler
comms->registerMessageHandler(MessageType::RESPONSE, 
    [](const CommMessagePtr& msg) -> bool {
        std::cout << "Received: " << msg->content << std::endl;
        return true;
    });

// Clean shutdown
comms->stop();
```

### AtomSpace Integration

```cpp
// Send AtomSpace content
Handle knowledge_atom = atomspace->add_node(CONCEPT_NODE, "Knowledge");
std::string msg_id = comms->sendAtomMessage(
    recipient,
    MessageType::KNOWLEDGE_SHARE,
    knowledge_atom
);

// Handler for AtomSpace messages
comms->registerMessageHandler(MessageType::KNOWLEDGE_SHARE,
    [](const CommMessagePtr& msg) -> bool {
        if (msg->atom_content != Handle::UNDEFINED) {
            // Process atom content
        }
        return true;
    });
```

### Broadcasting

```cpp
// Broadcast to multiple agents
std::vector<AgentId> recipients = {
    AgentId("Agent1"), AgentId("Agent2"), AgentId("Agent3")
};

size_t success_count = comms->broadcastMessage(
    recipients,
    MessageType::NOTIFICATION,
    "System maintenance in 5 minutes",
    MessagePriority::HIGH
);
```

## Building

### Prerequisites

- CMake 3.16+
- C++17 compiler
- OpenCog dependencies:
  - cogutil (required)
  - atomspace (required)
  - cogserver (optional, for network communication)

### Build Steps

```bash
# Configure
mkdir build && cd build
cmake .. -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON

# Build
make agentzero-communication

# Run tests
make test

# Run examples
./examples/basic_communication_example
```

### Installation

```bash
# Agent-Zero Communication Module - HumanInterface Layer

## Overview

The Agent-Zero Communication module implements **AZ-HUMAN-001: Create HumanInterface layer** as part of Phase 6: Communication & NLP in the Agent-Zero Genesis project. This module provides comprehensive human-agent interaction capabilities with deep OpenCog integration.

## Features

### Core Capabilities
- **Multi-modal Input Processing**: Support for text, voice, gesture, and multimodal inputs
- **Context-Aware Conversations**: Maintains conversation context and state across interactions
- **Session Management**: Handles multiple concurrent user sessions with proper isolation
- **AtomSpace Integration**: Stores interaction knowledge in OpenCog's AtomSpace
- **Real-time Analytics**: Monitors performance, success rates, and interaction patterns
- **Robust Error Handling**: Comprehensive error management with user-friendly messages

### Architecture Components

#### HumanInterface Class
The main orchestration class providing:
- Session lifecycle management
- Input processing and response generation  
- Context management across conversations
- Integration with OpenCog AtomSpace
- Performance monitoring and analytics

#### Supporting Components
- **InputProcessor**: Normalizes and processes various input modalities
- **ResponseGenerator**: Creates contextually appropriate responses
- **SessionManager**: Manages concurrent user sessions
- **ContextManager**: Maintains conversation context and state
- **InteractionKnowledgeStore**: Handles AtomSpace integration
- **InteractionAnalyzer**: Provides performance analytics

## Dependencies

### Required
- **cogutil** ≥ 2.0.3: OpenCog utilities and logging
- **atomspace** ≥ 5.0.4: Knowledge representation and storage
- **Boost** ≥ 1.70: System utilities and threading

### Optional
- **cogserver**: Enhanced integration and monitoring capabilities
- **lg-atomese**: Advanced natural language processing features

## Installation

### Build from Source

```bash
# Prerequisites: Install OpenCog dependencies
# See: https://github.com/opencog/cogutil
# See: https://github.com/opencog/atomspace

# Navigate to the Agent-Zero C++ directory
cd /path/to/pycog0/agents/cpp
# Agent-Zero Communication Module

## Overview

The Agent-Zero Communication Module provides natural language processing and dialogue management capabilities for the OpenCog Agent-Zero architecture. This module implements **AZ-NLP-002: Create DialogueManager for conversations** as part of Phase 6: Communication & NLP of the AGENT-ZERO-GENESIS project.

## 🎯 Key Features

- **Multi-turn Conversation Management**: Handle ongoing dialogues with context tracking
- **AtomSpace Integration**: Full integration with OpenCog's knowledge representation
- **Goal-Oriented Dialogue**: Support for pursuing conversational goals
- **Context Tracking**: Maintain conversation state and participant information
- **Message History**: Store and retrieve conversation history
- **Natural Language Processing**: Basic NLP with optional Link Grammar integration
- **Multi-Participant Support**: Handle group conversations

## 🏗️ Architecture

### Core Components

| Component | Purpose | Dependencies |
|-----------|---------|-------------|
| **DialogueManager** | Main conversation orchestration | AtomSpace, all other components |
| **ConversationState** | Individual conversation state management | AtomSpace |
| **LanguageProcessor** | Natural language processing | AtomSpace, lg-atomese (optional) |
| **MessageHandler** | Message routing and processing | AtomSpace |

### Key Classes

#### DialogueManager
The main class that orchestrates all conversation-related activities:
- Manages multiple concurrent conversations
- Provides conversation context and state tracking
- Integrates with OpenCog's AtomSpace for knowledge representation
- Supports goal-oriented dialogue management

#### ConversationState
Manages the state of individual conversations:
- Participant tracking
- Context variable management
- Topic tracking
- Activity monitoring

#### LanguageProcessor
Handles natural language processing tasks:
- Text parsing and understanding
- Response generation
- Intent detection
- Entity extraction

#### MessageHandler
Manages message processing and routing:
- Message validation
- AtomSpace representation
- Message history queries
- Callback management

## 🚀 Quick Start
The Agent-Zero Communication Module provides comprehensive natural language processing and communication capabilities for the Agent-Zero cognitive architecture. This module integrates Link Grammar parser with OpenCog's AtomSpace to enable sophisticated linguistic analysis and knowledge representation.

## Features

### LanguageProcessor (AZ-NLP-001) ✅ IMPLEMENTED

- **Syntactic Parsing**: Full Link Grammar integration for robust syntactic analysis
- **Semantic Analysis**: Entity extraction, concept identification, and sentiment analysis
- **AtomSpace Integration**: Seamless conversion of linguistic structures to AtomSpace representations
- **Multi-language Support**: Configurable for different languages (English, Russian, etc.)
- **Batch Processing**: Efficient processing of multiple texts with progress tracking
- **Performance Monitoring**: Comprehensive statistics and performance metrics
- **Error Handling**: Robust error handling with detailed error reporting

### Planned Components

- **DialogueManager** (AZ-NLP-002): Conversational interaction management
- **AgentComms** (AZ-COMM-001): Inter-agent communication protocols  
- **HumanInterface** (AZ-HUMAN-001): Human-agent interaction layer

## Dependencies

### Required OpenCog Components
- **cogutil**: Core OpenCog utilities
- **atomspace**: AtomSpace knowledge representation system
- **lg-atomese**: Atomese API for Link Grammar (recommended)
- **link-grammar**: Link Grammar natural language parser

### Optional Components
- **opencog**: Main OpenCog package for additional functionality

## Installation

### Prerequisites

Ensure you have the OpenCog ecosystem installed. From the repository root:

```bash
# Install dependencies
sudo apt-get install -y libboost-all-dev guile-3.0-dev build-essential
./scripts/adaptive-boost-install.sh

# Build foundation components
mkdir -p /tmp/opencog-build && cd /tmp/opencog-build
cmake /path/to/opencog-org
make cogutil atomspace lg-atomese link-grammar
sudo make install && sudo ldconfig
```

### Building the Module

```bash
# Navigate to the communication module
cd agents/cpp/agentzero-communication

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build the communication module
make agentzero-communication

# Run tests (optional)
make test

# Install system-wide (optional)
sudo make install
```

### CMake Integration

```cmake
find_package(AgentZeroCommunication REQUIRED)
target_link_libraries(your_target AgentZero::agentzero-communication)
```

# Configure
cmake ..

# Build
make -j$(nproc)

# Install (optional)
sudo make install
```

## Usage

### Basic Usage

```cpp
#include <opencog/atomspace/AtomSpace.h>
#include "opencog/agentzero/DialogueManager.h"

using namespace opencog;
using namespace opencog::agentzero;

// Create AtomSpace and DialogueManager
AtomSpacePtr atomspace = createAtomSpace();
DialogueManager dialogue_manager(atomspace, "MyAgent");

// Start a conversation
std::vector<std::string> participants = {"MyAgent", "Human"};
dialogue_manager.startConversation("conv1", participants);

// Process a message
std::string response = dialogue_manager.processMessage("conv1", "Human", "Hello!");
std::cout << "Agent response: " << response << std::endl;

// Set conversation context
dialogue_manager.setConversationTopic("conv1", "AI Research");
dialogue_manager.setConversationContext("conv1", "mood", "helpful");
```

### Advanced Features

```cpp
// Goal-oriented dialogue
Handle goal_atom = atomspace->add_node(CONCEPT_NODE, "ProvideAssistance");
dialogue_manager.addConversationGoal("conv1", goal_atom);

// Message history
auto history = dialogue_manager.getConversationHistory("conv1", 10);
for (const auto& message : history) {
    std::cout << message.sender_id << ": " << message.content << std::endl;
}

// Search messages
auto search_results = dialogue_manager.searchMessageHistory("conv1", "research");
```

## 🔧 Building

### Dependencies

**Required:**
- cogutil
- atomspace
- Boost libraries

**Optional:**
- lg-atomese (for enhanced language processing)
- link-grammar (for grammatical parsing)
- opencog (for additional NLP features)

### Build Instructions

```bash
# From the repository root
mkdir -p build && cd build
cmake ..
make agentzero-communication

# Install (optional)
sudo make install
sudo ldconfig
```

## Testing

The module includes comprehensive unit tests and integration tests:

```bash
# Run all communication tests
make test

# Run specific tests
make AgentCommsUTest_runner && ./AgentCommsUTest_runner
make MessageRouterUTest_runner && ./MessageRouterUTest_runner
make ProtocolManagerUTest_runner && ./ProtocolManagerUTest_runner
make MessageSerializerUTest_runner && ./MessageSerializerUTest_runner
```

## Examples

### Basic Communication
Demonstrates basic message sending and receiving between agents.

```bash
./examples/basic_communication_example
```

### Agent Messaging Demo
Shows advanced features like AtomSpace integration and broadcasting.

```bash
./examples/agent_messaging_demo
### Build with Tests

```bash
cmake -DBUILD_TESTING=ON ..
make
make test
```

## 🧪 Testing

The module includes comprehensive unit tests using CxxTest:

```bash
# Run all communication tests
cd build
make test

# Run specific test
./tests/DialogueManagerUTest
```

### Test Coverage

- Basic conversation management (start/end)
- Message processing and response generation
- Context and state management
- Goal-oriented dialogue features
- Message history and search
- AtomSpace integration
- Multi-conversation handling

## 📋 Examples

### Running the Example

```bash
cd build
make dialogue_manager_example
./examples/dialogue_manager_example
```

The example demonstrates:
- Basic conversation flow
- Multiple concurrent conversations
- Context tracking and management
- AtomSpace integration
- Goal-oriented dialogue

## 🔧 Configuration

### DialogueManager Settings

```cpp
// Set maximum conversation history
dialogue_manager.setMaxConversationHistory(1000);

// Set conversation timeout
dialogue_manager.setConversationTimeout(std::chrono::minutes(30));

// Enable/disable features
dialogue_manager.setContextTracking(true);
dialogue_manager.setGoalOrientedDialogue(true);
```

### Language Processing Options

```cpp
// Configure language processor
auto* lang_processor = dialogue_manager.getLanguageProcessor();
lang_processor->setUseLinks(true);  // Enable Link Grammar if available
lang_processor->setLanguageModel("/path/to/model");
```

## 📊 AtomSpace Integration

The module creates and maintains several types of atoms:

### Core Atoms
- **Agent Self Atom**: Represents the dialogue agent
- **Conversation Atoms**: Represent individual conversations
- **Message Atoms**: Represent individual messages
- **Context Atoms**: Store conversation context

### Relationship Links
- **Member Links**: Connect components to conversations
- **Evaluation Links**: Store properties and relationships
- **Ordered Links**: Represent message sequences

### Example AtomSpace Structure

```
(ConceptNode "MyAgent")
(ConceptNode "Conversation:conv1")
(ConceptNode "Message:msg_123")
(EvaluationLink
    (PredicateNode "manages")
    (ListLink
        (ConceptNode "MyAgent")
        (ConceptNode "Conversation:conv1")))
```

## 🔄 Integration with Other Modules

### Agent-Zero Core Integration

```cpp
// In AgentZeroCore
#include "opencog/agentzero/DialogueManager.h"

class AgentZeroCore {
private:
    std::unique_ptr<DialogueManager> _dialogue_manager;
    
public:
    void initializeCommunication() {
        _dialogue_manager = std::make_unique<DialogueManager>(
            _atomspace, _agent_name);
    }
    
    std::string processUserMessage(const std::string& message) {
        return _dialogue_manager->processMessage(
            "user_session", "Human", message);
    }
};
```

### Task Manager Integration

The DialogueManager can work with TaskManager to:
- Create tasks based on conversation goals
- Report task completion in dialogue
- Track conversational objectives

## 📈 Performance Considerations

- **Memory Usage**: Scales with conversation history and active conversations
- **Response Time**: Basic responses < 10ms, NLP processing varies
- **Concurrency**: Thread-safe for multiple concurrent conversations
- **AtomSpace Efficiency**: Uses efficient atom creation and querying

### Optimization Tips

1. **History Management**: Set appropriate `max_conversation_history`
2. **Context Pruning**: Remove unused context variables
3. **Goal Cleanup**: Remove completed conversational goals
4. **Inactive Conversations**: Use timeout to clean up old conversations

## 🔮 Future Enhancements

- **Enhanced NLP**: Integration with modern transformer models
- **Emotion Recognition**: Detect and respond to emotional states
- **Multi-Language Support**: Support for multiple languages
- **Voice Integration**: Speech-to-text and text-to-speech
- **Learning Capabilities**: Adaptive responses based on conversation history
- **Advanced Context**: Semantic context understanding

## 📚 API Reference

### DialogueManager Public Methods

#### Conversation Management
- `bool startConversation(id, participants)`
- `bool endConversation(id)`
- `bool isConversationActive(id)`
- `vector<string> getActiveConversations()`

#### Message Processing
- `string processMessage(conv_id, sender, content)`
- `bool sendMessage(conv_id, recipient, content)`
- `vector<Message> getConversationHistory(conv_id, limit=0)`
- `vector<Message> searchMessageHistory(conv_id, search_term)`

#### Context Management
- `void setConversationContext(conv_id, key, value)`
- `string getConversationContext(conv_id, key)`
- `void setConversationTopic(conv_id, topic)`
- `string getConversationTopic(conv_id)`

#### Goal Management
- `void addConversationGoal(conv_id, goal_atom)`
- `void removeConversationGoal(conv_id, goal_atom)`
- `vector<Handle> getConversationGoals(conv_id)`

#### AtomSpace Integration
- `AtomSpacePtr getAtomSpace()`
- `Handle conversationToAtom(conv_id)`
- `void updateDialogueAtoms()`

## 🛠️ Troubleshooting

### Common Issues

1. **Build Errors**
   - Ensure all dependencies are installed
   - Check pkg-config paths for OpenCog components

2. **Missing NLP Features**
   - Install lg-atomese for enhanced language processing
   - Install link-grammar for grammatical parsing

3. **Memory Issues**
   - Reduce `max_conversation_history`
   - Clean up inactive conversations regularly

4. **AtomSpace Errors**
   - Verify AtomSpace is properly initialized
   - Check atom creation and querying

### Debug Mode

```cpp
// Enable detailed logging
logger().set_level(Logger::DEBUG);
logger().set_print_to_stdout_flag(true);
```

## 🤝 Contributing

This module is part of the Agent-Zero project. Contributions should:

1. Follow OpenCog coding standards
2. Include comprehensive tests
3. Update documentation
4. Maintain AtomSpace integration
5. Consider performance implications

## 📄 License

SPDX-License-Identifier: AGPL-3.0-or-later

## 🔗 Related Documentation

- [AGENT-ZERO-GENESIS.md](../../../AGENT-ZERO-GENESIS.md): Complete project roadmap
- [Agent-Zero Core Module](../agentzero-core/README.md): Core orchestration engine
- [OpenCog AtomSpace Documentation](https://wiki.opencog.org/w/AtomSpace)
- [Link Grammar](https://www.abisource.com/projects/link-grammar/): Grammatical parsing
#include <agentzero/communication/HumanInterface.h>
#include <opencog/atomspace/AtomSpace.h>

// Create AtomSpace for knowledge representation
auto atomspace = std::make_shared<AtomSpace>();

// Configure the communication system
CommunicationConfig config;
config.enable_context_awareness = true;
config.max_concurrent_sessions = 50;

// Create and initialize HumanInterface
HumanInterface interface(atomspace, config);
interface.initialize();

// Start a user session
std::string session_id = interface.startSession("user_123");

// Process user input
HumanInput input("Hello, how can you help me?");
input.user_id = "user_123";

auto response = interface.processInput(input, session_id);

if (response.success) {
    std::cout << "Agent: " << response.agent_response.content << std::endl;
    std::cout << "Confidence: " << response.agent_response.confidence << std::endl;
}

// End session
interface.endSession(session_id);
```

### Advanced Context Management

```cpp
// Set conversation context
ContextUpdate context(ContextUpdate::UpdateType::SET, "topic", "machine_learning");
interface.updateContext(session_id, context);

// Get current context
auto current_context = interface.getContext(session_id);

// Process context-aware input
HumanInput input("Tell me more about this topic");
auto response = interface.processInput(input, session_id);
```

### Multi-Session Management

```cpp
// Handle multiple concurrent users
std::vector<std::string> sessions;

for (const auto& user_id : user_list) {
    std::string session_id = interface.startSession(user_id);
    sessions.push_back(session_id);
}

// Process inputs concurrently (thread-safe)
for (size_t i = 0; i < sessions.size(); ++i) {
    auto response = interface.processInput(user_inputs[i], sessions[i]);
    // Handle response...
}
```

### Analytics and Monitoring

```cpp
// Get performance analytics
auto analytics = interface.getAnalytics();
std::cout << "Total interactions: " << analytics.total_interactions << std::endl;
std::cout << "Average response time: " << analytics.average_response_time.count() << "ms" << std::endl;
std::cout << "Success rate: " << analytics.success_rate << std::endl;

// Monitor system status
auto status = interface.getStatus();
std::cout << "System healthy: " << status.healthy << std::endl;
std::cout << "Active sessions: " << status.active_sessions << std::endl;
```

## Configuration

### Communication Config Options

```cpp
CommunicationConfig config;

// Input processing
config.enable_input_preprocessing = true;
config.enable_context_awareness = true;

// Response settings  
config.default_output_format = OutputFormat::PLAIN_TEXT;
config.min_confidence_threshold = 0.3;
config.max_response_length = 1000;

// Session management
config.default_session_timeout = std::chrono::hours(1);
config.max_concurrent_sessions = 100;

// AtomSpace integration
config.store_interactions_in_atomspace = true;
config.enable_pattern_learning = true;

// Monitoring
config.enable_detailed_logging = false;
config.enable_performance_monitoring = true;
```

## API Reference

### Core Classes

#### HumanInterface
- `bool initialize()`: Initialize the interface system
- `std::string startSession(user_id, config)`: Start new session
- `InteractionResponse processInput(input, session_id)`: Process user input
- `bool endSession(session_id)`: End session
- `void updateContext(session_id, update)`: Update conversation context
- `InteractionAnalytics getAnalytics()`: Get performance metrics

#### Input/Output Types
- `HumanInput`: Structured input from users
- `InteractionResponse`: Agent responses with metadata
- `InteractionContext`: Conversation context and state
- `CommunicationConfig`: System configuration options

### Utility Functions
- `text::normalize()`: Text normalization
- `text::tokenize()`: Text tokenization
- `formatting::formatPlainText()`: Response formatting
- `session::generateSessionId()`: Session ID generation
- `analytics::calculatePercentiles()`: Performance analysis

## Examples

### Interactive Demo
```bash
# Build and run the demo
make human_interface_demo
./human_interface_demo
```

### Conversation Example  
```bash
# Build and run conversation example
make conversation_example
./conversation_example
```

## Testing

### Unit Tests
```bash
# Run all tests
make test

# Run specific test suites
./test_human_interface
./test_communication_utils
```

### Test Coverage
- Basic initialization and configuration
- Session lifecycle management
- Input processing and response generation
- Context management operations
- Multi-session concurrent handling
- Error handling and recovery
- Analytics and monitoring
- Utility function validation

## AtomSpace Integration

### Knowledge Representation
The HumanInterface stores interaction knowledge in AtomSpace:

```
(ConceptNode "User:user123")
(ConceptNode "Session:session_456") 
(ConceptNode "Input:hello world")
(ConceptNode "Response:hi there")
(ListLink (ConceptNode "User:user123") 
          (ConceptNode "Input:hello world")
          (ConceptNode "Response:hi there"))
```

### Pattern Learning
- Interaction patterns stored as atoms
- Truth values represent confidence and frequency
- Temporal relationships captured with TimeNodes
- Context stored as structured atom relationships

## Performance Characteristics

### Benchmarks (OpenCog AtomSpace integration)
- **Initialization**: < 100ms with empty AtomSpace
- **Session creation**: < 10ms per session
- **Input processing**: 50-200ms depending on complexity
- **Context updates**: < 5ms per operation
- **Memory usage**: ~50MB base + 1KB per active session
- **Concurrent sessions**: Tested up to 1000 simultaneous sessions

### Scalability
- Linear scaling with number of sessions
- AtomSpace operations scale with knowledge base size
- Thread-safe for concurrent access
- Configurable resource limits

## Integration with Agent-Zero

### Core Module Dependencies
- Integrates with `agentzero-core` for base agent functionality
- Uses shared AtomSpace with other Agent-Zero components
- Follows Agent-Zero architectural patterns

### Future Integration Points
- **AZ-NLP-001**: Language processing will enhance input understanding
- **AZ-NLP-002**: Dialogue management will use HumanInterface sessions
- **AZ-COMM-001**: Agent communication protocols will build on this foundation
#include <opencog/atomspace/AtomSpace.h>
#include "agentzero/communication/LanguageProcessor.h"

using namespace agentzero::communication;
using namespace opencog;

int main() {
    // Create AtomSpace
    auto atomspace = std::make_shared<AtomSpace>();
    
    // Configure LanguageProcessor
    LanguageProcessorConfig config = create_english_config();
    config.store_in_atomspace = true;
    config.enable_semantic_analysis = true;
    
    // Create processor
    LanguageProcessor processor(atomspace, config);
    
    // Parse text
    ParseResult result = processor.parse_text("The cat sat on the mat.");
    
    if (result.is_valid) {
        std::cout << "Parse successful!" << std::endl;
        std::cout << "Words: " << result.words.size() << std::endl;
        std::cout << "Links: " << result.links.size() << std::endl;
    }
    
    return 0;
}
```

### Batch Processing

```cpp
// Process multiple sentences efficiently
std::vector<std::string> sentences = {
    "Hello world.",
    "How are you today?",
    "The weather is nice."
};

// Set progress callback (optional)
processor.set_progress_callback([](size_t current, size_t total) {
    std::cout << "Progress: " << current << "/" << total << std::endl;
});

// Process batch
auto results = processor.parse_batch(sentences);

// Analyze results
for (const auto& result : results) {
    if (result.is_valid) {
        std::cout << "✓ " << result.text << std::endl;
    } else {
        std::cout << "✗ " << result.text << std::endl;
    }
}
```

### Semantic Analysis

```cpp
// Comprehensive semantic analysis
std::string text = "John loves reading books about artificial intelligence.";
SemanticResult semantic = processor.analyze_text(text);

// Access extracted information
std::cout << "Entities: ";
for (const std::string& entity : semantic.entities) {
    std::cout << entity << " ";
}

std::cout << "\nConcepts: ";
for (const std::string& concept : semantic.concepts) {
    std::cout << concept << " ";
}

// Sentiment scores
for (const auto& sentiment : semantic.sentiment_scores) {
    std::cout << sentiment.first << ": " << sentiment.second << std::endl;
}
```

## Configuration

### CommConfig Options

```cpp
CommConfig config;
config.enable_network = false;              // Network communication
config.enable_persistence = true;           // AtomSpace persistence
config.enable_compression = false;          // Message compression
config.max_message_size = 1024 * 1024;     // 1MB max message size
config.message_timeout = std::chrono::seconds(30);
config.max_queue_size = 1000;              // Max messages in queue
config.network_address = "127.0.0.1";      // Network binding
config.network_port = 17001;               // Network port
```

## Performance Characteristics

### Message Delivery Times (Estimated)
- **Local Protocol**: < 1ms
- **IPC Protocol**: 5-10ms
- **Network Protocol**: 50-100ms
- **Broadcast**: Varies by protocol and recipient count

### Memory Usage
- Base overhead: ~1MB per AgentComms instance
- Message storage: ~200 bytes + content size per message
- Route cache: ~50 bytes per cached route

### Scalability
- Local agents: 1000+ agents per process
- Network agents: Limited by network capacity
- Message throughput: 10,000+ messages/second (local)

## Integration with OpenCog

The communication module follows OpenCog architectural patterns:

- Uses AtomSpace for knowledge representation
- Integrates with CogServer for network operations
- Follows OpenCog naming conventions and coding standards
- Compatible with existing OpenCog modules

## Error Handling

The module provides robust error handling:

- Message validation and format checking
- Protocol failure detection and recovery
- Network disconnection handling
- Resource exhaustion protection
- Comprehensive logging and debugging support

## Thread Safety

All public methods are thread-safe and can be called concurrently from multiple threads. Internal synchronization ensures data integrity and prevents race conditions.

## Future Enhancements

Potential future improvements:

- Message encryption and security
- Advanced compression algorithms
- Protocol plugins and extensions
- Distributed agent discovery
- Quality of Service (QoS) guarantees
- Message persistence to disk
- Integration with external message brokers

## API Documentation

For detailed API documentation, see the header files:
- `AgentComms.h` - Main communication interface
- `MessageRouter.h` - Message routing functionality
- `ProtocolManager.h` - Protocol management
- `MessageSerializer.h` - Message serialization
- `CommTypes.h` - Common types and utilities

## Dependencies

### Required
- **cogutil**: Core OpenCog utilities
- **atomspace**: Knowledge representation
- **Boost**: System libraries and threading

### Optional
- **cogserver**: Network communication capabilities
- **CxxTest**: Unit testing framework

## License

SPDX-License-Identifier: AGPL-3.0-or-later

Part of the OpenCog Foundation's Agent-Zero-Genesis project.
### LanguageProcessorConfig Options

```cpp
LanguageProcessorConfig config;
config.language = "en";                    // Language code
config.dictionary_path = "";               // Custom dictionary path (optional)
config.max_parse_time = 10;               // Maximum parse time (seconds)
config.verbosity_level = 0;               // Link Grammar verbosity (0-6)
config.store_in_atomspace = true;         // Store results in AtomSpace
config.enable_semantic_analysis = true;   // Enable semantic processing
config.confidence_threshold = 0.5;        // Minimum confidence for results
```

### Predefined Configurations

```cpp
// English configuration
auto en_config = create_english_config();

// Russian configuration  
auto ru_config = create_russian_config();

// Validate configuration
if (validate_config(config)) {
    // Configuration is valid
}
```

## Testing

### Running Unit Tests

```bash
# From build directory
make test

# Or run individual test executables
./tests/test_language_processor
./tests/test_communication_basic
```

### Test Coverage

- **Initialization and Configuration**: Verify proper setup and configuration handling
- **Basic Parsing**: Test simple sentence parsing functionality
- **Batch Processing**: Validate batch processing capabilities
- **Semantic Analysis**: Test entity and concept extraction
- **AtomSpace Integration**: Verify AtomSpace representation creation
- **Error Handling**: Test robustness with invalid inputs
- **Performance**: Benchmark parsing speed and memory usage

## Examples

See the `examples/` directory for comprehensive usage examples:

- `language_processor_example.cpp`: Basic LanguageProcessor usage
- `batch_processing_example.cpp`: Batch processing demonstration

### Running Examples

```bash
# From build directory
./examples/language_processor_example
./examples/batch_processing_example 100  # Process 100 sentences
```

## Architecture

### Class Hierarchy

```
LanguageProcessor
├── ParseResult (struct)
├── SemanticResult (struct)  
├── LanguageProcessorConfig (struct)
└── Utility functions

DialogueManager (planned)
AgentComms (planned)
HumanInterface (planned)
```

### AtomSpace Integration

The LanguageProcessor creates rich AtomSpace representations:

- **Sentence Nodes**: Represent complete sentences
- **Word Nodes**: Individual words with position information
- **Link Relations**: Syntactic relationships between words
- **Concept Nodes**: Extracted semantic concepts
- **Metadata**: Parse costs, confidence scores, timestamps

### Error Handling

- **Input Validation**: Comprehensive input checking
- **Resource Management**: Proper cleanup of Link Grammar resources
- **Exception Safety**: RAII and exception-safe design
- **Error Reporting**: Detailed error messages and codes

## Performance

### Benchmarks

Typical performance on modern hardware:

- **Simple Sentences**: 10-50ms per sentence
- **Complex Sentences**: 50-200ms per sentence  
- **Batch Processing**: 20-30% faster than individual processing
- **Memory Usage**: ~1-5MB per 1000 parsed sentences

### Optimization Tips

1. Use batch processing for multiple sentences
2. Set appropriate `max_parse_time` limits
3. Disable verbose output in production
4. Reuse processor instances when possible
5. Monitor statistics for performance insights

## Troubleshooting

### Common Issues

#### Build Errors
```bash
# Missing OpenCog dependencies
sudo apt-get install libcogutil-dev libatomspace-dev

# Or build from source:
# https://github.com/opencog/cogutil
# https://github.com/opencog/atomspace
```

#### Runtime Issues
```cpp
// AtomSpace not accessible
if (!interface.initialize()) {
    std::cerr << "Check AtomSpace availability" << std::endl;
}

// Session not found errors
if (!interface.validateSession(session_id)) {
    std::cerr << "Session expired or invalid" << std::endl;
}
```

### Logging and Debugging
```cpp
// Enable detailed logging
config.enable_detailed_logging = true;

// Check system status
auto status = interface.getStatus();
for (const auto& warning : status.warnings) {
    std::cout << "Warning: " << warning << std::endl;
}
**"Link Grammar not found"**
```bash
# Install Link Grammar development package
sudo apt-get install -y liblink-grammar-dev

# Or build from source
git clone https://github.com/opencog/link-grammar
cd link-grammar && mkdir build && cd build
cmake .. && make && sudo make install
```

**"Parse always fails"**
- Check Link Grammar dictionary installation
- Verify language code is correct
- Try increasing `max_parse_time`
- Check verbosity output for debugging

**"AtomSpace integration not working"**
- Ensure `store_in_atomspace` is enabled
- Verify AtomSpace is properly initialized
- Check for proper linking of atomspace library

### Debug Mode

Enable debugging by setting verbosity level:

```cpp
config.verbosity_level = 2;  // Show timing info
config.verbosity_level = 6;  // Show detailed parsing info
```

## Contributing

### Development Guidelines
- Follow OpenCog coding standards
- Add unit tests for new features
- Update documentation for API changes
- Use AtomSpace integration patterns

### Architecture Notes
- Thread-safe design for concurrent access
- Modular components for extensibility
- Comprehensive error handling
- Performance monitoring built-in

## License

Copyright (C) 2024 OpenCog Foundation
SPDX-License-Identifier: AGPL-3.0-or-later

## Related Documentation

- [AGENT-ZERO-GENESIS.md](../../../AGENT-ZERO-GENESIS.md) - Complete project roadmap
- [Agent-Zero Core Module](../agentzero-core/README.md) - Base functionality
- [OpenCog AtomSpace](https://github.com/opencog/atomspace) - Knowledge representation
- [Phase 6 Tasks](../../../AGENT-ZERO-GENESIS.md#phase-6-communication--nlp) - Communication & NLP roadmap
This module follows OpenCog architectural patterns and coding standards:

1. **C++17 Standard**: Use modern C++ features appropriately
2. **RAII**: Proper resource management
3. **Exception Safety**: Handle errors gracefully  
4. **Documentation**: Comprehensive API documentation
5. **Testing**: Unit tests for all public interfaces
6. **Performance**: Optimize for production use

## Roadmap

### Phase 6 - Communication & NLP (Current)
- ✅ **AZ-NLP-001**: LanguageProcessor with Link Grammar (COMPLETED)
- 🔄 **AZ-NLP-002**: DialogueManager for conversations  
- 🔄 **AZ-COMM-001**: AgentComms protocols
- 🔄 **AZ-HUMAN-001**: HumanInterface layer

### Future Enhancements
- Multi-threading support for batch processing
- Plugin architecture for custom semantic analyzers
- Real-time parsing capabilities
- Integration with speech recognition/synthesis
- Advanced dialogue state tracking
- Distributed agent communication protocols

## License

This module is part of the Agent-Zero-Genesis project and is licensed under the GNU Affero General Public License v3. See the LICENSE file in the repository root for details.

## References

- [Link Grammar Documentation](https://www.abisource.com/projects/link-grammar/)
- [OpenCog AtomSpace](https://wiki.opencog.org/w/AtomSpace)
- [Agent-Zero Genesis](../../AGENT-ZERO-GENESIS.md)
- [OpenCog CMake Build Guide](../../../CMAKE_BUILD_GUIDE.md)
