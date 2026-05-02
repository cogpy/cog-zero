# Agent-Zero Integration Guide

Guide for integrating Agent-Zero with OpenCog components and external systems.

## Table of Contents

1. [AtomSpace Integration](#atomspace-integration)
2. [CogServer Integration](#cogserver-integration)
3. [PLN Reasoning](#pln-reasoning)
4. [MOSES Learning](#moses-learning)
5. [External Tools](#external-tools)
6. [Custom Integrations](#custom-integrations)

## AtomSpace Integration

### Basic AtomSpace Usage

```cpp
#include <opencog/atomspace/AtomSpace.h>
#include <agentzero/AgentZeroCore.h>

using namespace opencog;
using namespace agentzero;

// Create AtomSpace
AtomSpace atomspace;

// Create agent with AtomSpace
AgentZeroCore agent(atomspace);
```

### Representing Agent State

All cognitive state should be represented as Atoms:

```cpp
// Goals as EvaluationLinks
Handle createGoal(AtomSpace& as, const std::string& description, float priority) {
    Handle pred = as.add_node(PREDICATE_NODE, "Goal");
    Handle concept = as.add_node(CONCEPT_NODE, description);
    Handle priorityNode = as.add_node(NUMBER_NODE, std::to_string(priority));
    
    return as.add_link(EVALUATION_LINK,
        pred,
        as.add_link(LIST_LINK, concept, priorityNode));
}

// Knowledge as InheritanceLinks
Handle assertKnowledge(AtomSpace& as, const std::string& instance, 
                       const std::string& category) {
    Handle inst = as.add_node(CONCEPT_NODE, instance);
    Handle cat = as.add_node(CONCEPT_NODE, category);
    
    return as.add_link(INHERITANCE_LINK, inst, cat);
}

// Actions as ExecutionLinks
Handle createAction(AtomSpace& as, const std::string& actionName,
                   const HandleSeq& args) {
    Handle schema = as.add_node(SCHEMA_NODE, actionName);
    Handle argList = as.add_link(LIST_LINK, args);
    
    return as.add_link(EXECUTION_LINK, schema, argList);
}
```

### Pattern Matching

Use AtomSpace pattern matching for querying:

```cpp
// Find all goals
Handle findAllGoals(AtomSpace& as) {
    Handle varGoal = as.add_node(VARIABLE_NODE, "$goal");
    Handle varPriority = as.add_node(VARIABLE_NODE, "$priority");
    Handle pred = as.add_node(PREDICATE_NODE, "Goal");
    
    Handle pattern = as.add_link(EVALUATION_LINK,
        pred,
        as.add_link(LIST_LINK, varGoal, varPriority));
    
    Handle varList = as.add_link(VARIABLE_LIST, varGoal, varPriority);
    
    return as.add_link(GET_LINK, varList, pattern);
}

// Execute pattern matching
HandleSeq matches = satisfying_set(&atomspace, findAllGoals(atomspace));
```

### Truth Values

Work with uncertainty using TruthValues:

```cpp
#include <opencog/truthvalue/SimpleTruthValue.h>

// Set truth value
Handle atom = /* ... */;
TruthValuePtr tv = SimpleTruthValue::createTV(0.8, 0.9);  // strength, confidence
atom->setTruthValue(tv);

// Read truth value
TruthValuePtr currentTv = atom->getTruthValue();
double strength = currentTv->get_mean();
double confidence = currentTv->get_confidence();
```

## CogServer Integration

### Running as CogServer Module

```cpp
#include <opencog/cogserver/server/CogServer.h>
#include <agentzero/AgentZeroModule.h>

class AgentZeroModule : public Module {
private:
    std::unique_ptr<AgentZeroCore> agent;
    
public:
    void init() override {
        // Get CogServer's AtomSpace
        AtomSpace& as = cogserver().getAtomSpace();
        
        // Create agent
        agent = std::make_unique<AgentZeroCore>(as);
        agent->initialize();
        
        // Register commands
        registerCommand("agent-step", &AgentZeroModule::cmdStep);
        registerCommand("agent-status", &AgentZeroModule::cmdStatus);
        registerCommand("agent-goal", &AgentZeroModule::cmdGoal);
    }
    
    void run(CogServer* server) override {
        // Run cognitive loop
        if (agent && agent->isInitialized()) {
            agent->cognitiveStep();
        }
    }
    
private:
    std::string cmdStep(Request* req, std::vector<std::string>& args) {
        agent->cognitiveStep();
        return "Agent step completed\n";
    }
    
    std::string cmdStatus(Request* req, std::vector<std::string>& args) {
        auto stats = agent->getStatistics();
        std::stringstream ss;
        ss << "Agent Status:\n";
        ss << "  Steps: " << stats.totalSteps << "\n";
        ss << "  Goals: " << stats.activeGoals << "\n";
        return ss.str();
    }
};

// Module registration
DECLARE_MODULE(AgentZeroModule);
```

### CogServer Commands

Create custom telnet commands:

```cpp
class AgentGoalCommand : public Request {
public:
    AgentGoalCommand(CogServer& cs) : Request(cs) {}
    
    bool execute() override {
        std::string description = _parameters[0];
        float priority = std::stof(_parameters[1]);
        
        auto& module = cogserver().getModule<AgentZeroModule>();
        Handle goal = module.getAgent()->addGoal(description, priority);
        
        _output << "Created goal: " << goal->to_short_string() << "\n";
        return true;
    }
};
```

## PLN Reasoning

### Custom PLN Rules

Define custom reasoning rules for Agent-Zero:

```cpp
#include <opencog/ure/Rule.h>
#include <opencog/pln/rules/rule-utils.h>

// Goal decomposition rule
Handle createGoalDecompositionRule(AtomSpace& as) {
    // Variables
    Handle varGoal = as.add_node(VARIABLE_NODE, "$goal");
    Handle varSubgoal1 = as.add_node(VARIABLE_NODE, "$subgoal1");
    Handle varSubgoal2 = as.add_node(VARIABLE_NODE, "$subgoal2");
    
    // Pattern: Goal that can be decomposed
    Handle pattern = as.add_link(AND_LINK,
        as.add_link(EVALUATION_LINK,
            as.add_node(PREDICATE_NODE, "Goal"),
            varGoal),
        as.add_link(EVALUATION_LINK,
            as.add_node(PREDICATE_NODE, "Decomposable"),
            varGoal));
    
    // Rewrite: Create subgoals
    Handle rewrite = as.add_link(EXECUTION_OUTPUT_LINK,
        as.add_node(GROUNDED_SCHEMA_NODE, "scm: goal-decomposition"),
        as.add_link(LIST_LINK, varGoal));
    
    // Bind them together
    return as.add_link(BIND_LINK,
        as.add_link(VARIABLE_LIST, varGoal, varSubgoal1, varSubgoal2),
        pattern,
        rewrite);
}

// Register with URE
void registerAgentRules(AtomSpace& as) {
    Handle ruleBase = as.add_node(CONCEPT_NODE, "AgentZeroRuleBase");
    Handle rule = createGoalDecompositionRule(as);
    
    // Add rule to URE
    Handle ruleAlias = as.add_node(DEFINED_SCHEMA_NODE, "GoalDecomposition");
    as.add_link(DEFINE_LINK, ruleAlias, rule);
    
    // Set rule weight
    Handle memberLink = as.add_link(MEMBER_LINK, ruleAlias, ruleBase);
    memberLink->setTruthValue(SimpleTruthValue::createTV(0.9, 1.0));
}
```

### Using PLN for Inference

```cpp
#include <opencog/pln/BackwardChainer.h>

void performInference(AtomSpace& as, Handle target) {
    // Configure backward chainer
    Handle ruleBase = as.add_node(CONCEPT_NODE, "AgentZeroRuleBase");
    BackwardChainer bc(as, ruleBase, target);
    
    // Set parameters
    bc.set_maximum_iterations(100);
    bc.set_complexity_penalty(1.0);
    
    // Run inference
    bc.do_chain();
    
    // Get results
    HandleSet results = bc.get_results();
    for (Handle h : results) {
        std::cout << "Result: " << h->to_string() << std::endl;
        std::cout << "TV: " << h->getTruthValue()->to_string() << std::endl;
    }
}
```

## MOSES Learning

### Policy Optimization with MOSES

```cpp
#include <opencog/moses/moses/moses.h>
#include <agentzero/PolicyOptimizer.h>

class PolicyFitnessFunction {
    std::vector<Experience> trainingData;
    
public:
    PolicyFitnessFunction(const std::vector<Experience>& data) 
        : trainingData(data) {}
    
    double operator()(const combo::combo_tree& policy) const {
        double totalReward = 0.0;
        int successCount = 0;
        
        for (const auto& exp : trainingData) {
            // Evaluate policy on experience
            auto result = evaluatePolicy(policy, exp);
            totalReward += result.reward;
            if (result.success) successCount++;
        }
        
        // Fitness combines reward and success rate
        double avgReward = totalReward / trainingData.size();
        double successRate = double(successCount) / trainingData.size();
        
        return 0.7 * avgReward + 0.3 * successRate;
    }
};

// Use MOSES to evolve policies
combo::combo_tree evolvePolicy(const std::vector<Experience>& data) {
    using namespace opencog::moses;
    
    // Create fitness function
    PolicyFitnessFunction fitness(data);
    
    // Configure MOSES
    moses_parameters params;
    params.max_evals = 10000;
    params.pop_size = 100;
    params.max_gens = 50;
    
    // Create metapopulation
    metapopulation metapop;
    
    // Run evolution
    metapop.run(fitness, params);
    
    // Return best policy
    return metapop.best_candidate();
}
```

### Online Learning Integration

```cpp
void integrateOnlineLearning(AgentZeroCore& agent) {
    auto learningEngine = agent.getComponent<LearningEngine>("learning");
    
    // Set up continuous learning callback
    agent.setCognitiveStepCallback([&learningEngine](const CognitiveState& state) {
        // Collect experience from current step
        Experience exp = {
            .context = state.context,
            .action = state.lastAction,
            .outcome = state.outcome,
            .reward = state.reward
        };
        
        // Store experience
        learningEngine->storeExperience(exp);
        
        // Periodically update policy
        if (learningEngine->shouldUpdate()) {
            learningEngine->updatePolicy();
        }
    });
}
```

## External Tools

### Tool Integration Framework

```cpp
#include <agentzero/ToolRegistry.h>

// Define a tool
class CalculatorTool {
public:
    Result execute(const Args& args) {
        std::string operation = args.get<std::string>("operation");
        double a = args.get<double>("a");
        double b = args.get<double>("b");
        
        double result;
        if (operation == "add") result = a + b;
        else if (operation == "subtract") result = a - b;
        else if (operation == "multiply") result = a * b;
        else if (operation == "divide") result = a / b;
        else throw InvalidArgumentException("Unknown operation");
        
        return Result::success(result);
    }
};

// Register tool
void registerTools(ToolRegistry& registry) {
    Tool calcTool {
        .name = "calculator",
        .description = "Performs basic arithmetic operations",
        .capabilities = {Capability::Arithmetic},
        .invoke = [](const Args& args) {
            CalculatorTool calc;
            return calc.execute(args);
        }
    };
    
    registry.registerTool(calcTool);
}

// Use tool
void useTool(AgentZeroCore& agent, const std::string& toolName) {
    auto toolSystem = agent.getComponent<ToolRegistry>("tools");
    
    Args args;
    args.set("operation", "add");
    args.set("a", 5.0);
    args.set("b", 3.0);
    
    Result result = toolSystem->invokeTool(toolName, args);
    if (result.success) {
        std::cout << "Result: " << result.getValue<double>() << std::endl;
    }
}
```

### ROS Integration Example

```cpp
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <agentzero/AgentZeroCore.h>

class ROSInterface {
    ros::NodeHandle nh;
    ros::Subscriber sub;
    ros::Publisher pub;
    AgentZeroCore& agent;
    
public:
    ROSInterface(AgentZeroCore& agent) : agent(agent) {
        // Subscribe to sensor topic
        sub = nh.subscribe("/sensors/data", 10, 
                          &ROSInterface::sensorCallback, this);
        
        // Publish to action topic
        pub = nh.advertise<std_msgs::String>("/agent/actions", 10);
    }
    
    void sensorCallback(const std_msgs::String::ConstPtr& msg) {
        // Convert ROS message to sensory data
        SensoryData data = convertFromROS(msg);
        
        // Process with agent
        auto perceiver = agent.getComponent<PerceptualProcessor>("perception");
        perceiver->process(data);
    }
    
    void publishAction(const Action& action) {
        std_msgs::String msg;
        msg.data = serializeAction(action);
        pub.publish(msg);
    }
};
```

## Custom Integrations

### Creating Custom Components

```cpp
#include <agentzero/Component.h>

class CustomComponent : public Component {
    AtomSpace& atomspace;
    
public:
    explicit CustomComponent(AtomSpace& as) : atomspace(as) {}
    
    void initialize() override {
        // Initialization logic
    }
    
    void process() override {
        // Main processing logic
        // Access AtomSpace
        HandleSeq atoms = atomspace.get_atoms_by_type(CONCEPT_NODE);
        
        // Perform custom operations
        for (Handle h : atoms) {
            // Process each atom
        }
    }
    
    void shutdown() override {
        // Cleanup logic
    }
};

// Register with agent
void registerCustomComponent(AgentZeroCore& agent) {
    auto component = std::make_shared<CustomComponent>(agent.getAtomSpace());
    agent.registerComponent("custom", component);
}
```

### Extending the Cognitive Loop

```cpp
void extendCognitiveLoop(AgentZeroCore& agent) {
    auto cogLoop = agent.getComponent<CognitiveLoop>("cognitive-loop");
    
    // Add custom phase handler
    cogLoop->setCustomPhaseHandler([&agent]() {
        // Custom cognitive processing
        auto custom = agent.getComponent<CustomComponent>("custom");
        custom->process();
    });
    
    // Modify phase order
    cogLoop->configure(CognitiveConfig{
        .phases = {
            Phase::Perception,
            Phase::Attention,
            Phase::Custom,      // Insert custom phase
            Phase::Reasoning,
            Phase::Planning,
            Phase::Action,
            Phase::Learning,
            Phase::Reflection
        }
    });
}
```

## Integration Patterns

### Pattern 1: Sensor-Perception-Action

Complete integration example:

```cpp
class SensorPerceptionActionLoop {
    AgentZeroCore& agent;
    SensorInterface& sensors;
    ActuatorInterface& actuators;
    
public:
    void run() {
        while (true) {
            // 1. Read sensors
            SensoryData data = sensors.read();
            
            // 2. Process perception
            auto perceiver = agent.getComponent<PerceptualProcessor>("perception");
            perceiver->process(data);
            
            // 3. Run cognitive step
            agent.cognitiveStep();
            
            // 4. Execute actions
            auto scheduler = agent.getComponent<ActionScheduler>("actions");
            auto actions = scheduler->getReadyActions();
            
            for (const Action& action : actions) {
                actuators.execute(action);
            }
            
            // 5. Collect feedback
            Outcome outcome = sensors.readOutcome();
            agent.provideFeedback(outcome);
        }
    }
};
```

### Pattern 2: Multi-Agent Communication

```cpp
class MultiAgentSystem {
    std::vector<std::unique_ptr<AgentZeroCore>> agents;
    MessageBroker broker;
    
public:
    void step() {
        // Each agent processes
        for (auto& agent : agents) {
            agent->cognitiveStep();
            
            // Share knowledge
            auto knowledge = agent->exportKnowledge();
            broker.publish(agent->getId(), knowledge);
        }
        
        // Distribute messages
        for (auto& agent : agents) {
            auto messages = broker.getMessages(agent->getId());
            for (const auto& msg : messages) {
                agent->receiveMessage(msg);
            }
        }
    }
};
```

## Best Practices

1. **AtomSpace-First**: Keep all state in AtomSpace
2. **Handle Lifetime**: AtomSpace manages atom lifetime
3. **Thread Safety**: Use AtomSpace's built-in thread safety
4. **Error Handling**: Always handle OpenCog exceptions
5. **Performance**: Minimize AtomSpace queries in hot loops
6. **Testing**: Test integrations with mock OpenCog components

## Troubleshooting

### Common Integration Issues

**"Cannot find OpenCog libraries"**
```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
sudo ldconfig
```

**"AtomSpace atom type unknown"**
- Ensure proper type loading
- Check atom type definitions

**"URE rule not firing"**
- Verify rule syntax
- Check rule weights
- Increase max iterations

For more help, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

---

*Part of the AGENT-ZERO-GENESIS documentation - Phase 9: Integration & Testing (AZ-DOC-001)*
