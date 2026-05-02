/*
 * Simple build test for AgentZero Core
 * Tests basic compilation without running full tests
 */

#include <iostream>
#include <memory>

// Mock the dependencies for testing compilation
namespace opencog {
    class CogServer {
    public:
        std::shared_ptr<void> getAtomSpace() { return nullptr; }
    };
    
    class Module {
    protected:
        CogServer& _cogserver;
    public:
        Module(CogServer& cs) : _cogserver(cs) {}
        virtual ~Module() {}
        virtual void init() = 0;
        virtual bool config(const char*) = 0;
    };
}

// Include our headers (this tests compilation)
#include "opencog/agentzero/AgentZeroCore.h"

using namespace opencog;
using namespace opencog::agentzero;

int main() {
    std::cout << "AgentZero Core compilation test" << std::endl;
    
    // Test basic instantiation
    CogServer cogserver;
    AgentZeroCore agent(cogserver, "TestAgent");
    
    std::cout << "Agent name: " << agent.getAgentName() << std::endl;
    std::cout << "Running: " << agent.isRunning() << std::endl;
    std::cout << "Initialized: " << agent.isInitialized() << std::endl;
    
    std::cout << "Build test completed successfully!" << std::endl;
    return 0;
}