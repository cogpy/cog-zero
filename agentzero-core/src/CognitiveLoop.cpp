/*
 * agentzero-core/src/CognitiveLoop.cpp
 *
 * OpenCog AtomSpace-backed cognitive loop implementation.
 */
#include "opencog/agentzero/CognitiveLoop.h"
#include "opencog/agentzero/AgentZeroCore.h"
#include "opencog/agentzero/TaskManager.h"
#include "opencog/agentzero/KnowledgeIntegrator.h"
#include "opencog/agentzero/ReasoningEngine.h"

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <algorithm>

using namespace opencog;
using namespace opencog::agentzero;

CognitiveLoop::CognitiveLoop(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(std::move(atomspace))
{
    if (!_atomspace) {
        throw std::invalid_argument("CognitiveLoop requires a valid AtomSpace");
    }
    _cycle_root = _atomspace->add_node(CONCEPT_NODE, "CognitiveLoop");
}

CognitiveLoop::~CognitiveLoop()
{
    stop();
}

bool CognitiveLoop::start()
{
    if (_running.load()) return true;
    _running = true;
    if (_thread.joinable()) {
        _thread.join();
    }
    _thread = std::thread([this]() { loopBody(); });
    logger().info() << "[CognitiveLoop] started";
    return true;
}

bool CognitiveLoop::stop()
{
    if (!_running.load()) {
        if (_thread.joinable()) _thread.join();
        return true;
    }
    _running = false;
    if (_thread.joinable()) _thread.join();
    logger().info() << "[CognitiveLoop] stopped";
    return true;
}

void CognitiveLoop::runSingleCycle()
{
    auto t0 = std::chrono::steady_clock::now();
    CycleStats stats;
    stats.cycle_number = _cycle_count.load() + 1;
    stats.percepts_processed = phasePerception();
    stats.conclusions = phaseReasoning();
    stats.tasks_processed = phasePlanning() + phaseAction();
    auto t1 = std::chrono::steady_clock::now();
    stats.duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    phaseReflection(stats);
    _cycle_count.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(_stats_mu);
        _last_stats = stats;
    }
}

CognitiveLoop::CycleStats CognitiveLoop::lastStats() const
{
    std::lock_guard<std::mutex> lock(_stats_mu);
    return _last_stats;
}

void CognitiveLoop::addPercept(const Percept& p)
{
    std::lock_guard<std::mutex> lock(_percept_mu);
    _pending_percepts.push_back(p);
}

void CognitiveLoop::addPercept(const std::string& source, const std::string& content,
                               const std::string& modality, double salience)
{
    Percept p;
    p.source = source;
    p.content = content;
    p.modality = modality;
    p.salience = salience;
    addPercept(p);
}

void CognitiveLoop::loopBody()
{
    while (_running.load()) {
        if (_max_cycles > 0 && _cycle_count.load() >= _max_cycles) {
            _running = false;
            break;
        }
        runSingleCycle();
        std::this_thread::sleep_for(_cycle_interval);
    }
}

size_t CognitiveLoop::phasePerception()
{
    std::vector<Percept> batch;
    {
        std::lock_guard<std::mutex> lock(_percept_mu);
        batch.swap(_pending_percepts);
    }
    size_t n = 0;
    for (auto& p : batch) {
        p.atom = encodePercept(p);
        _last_percept_atom = p.atom;
        ++n;
    }
    return n;
}

size_t CognitiveLoop::phaseReasoning()
{
    if (!_agent_core) return 0;
    auto* engine = _agent_core->getReasoningEngine();
    if (!engine) return 0;
    if (!engine->processReasoningCycle()) return 0;
    _last_conclusion_atom = engine->getLastConclusion();
    return engine->getConclusionCount() > 0 ? 1 : 0;
}

size_t CognitiveLoop::phasePlanning()
{
    if (!_agent_core) return 0;
    auto* tm = _agent_core->getTaskManager();
    if (!tm) return 0;
    return tm->processTaskManagement() ? 1 : 0;
}

size_t CognitiveLoop::phaseAction()
{
    if (!_agent_core) return 0;
    auto* tm = _agent_core->getTaskManager();
    if (!tm) return 0;
    return tm->executeAll();
}

void CognitiveLoop::phaseReflection(const CycleStats& stats)
{
    if (!_atomspace) return;
    Handle cycle_node = _atomspace->add_node(
        CONCEPT_NODE, "Cycle_" + std::to_string(stats.cycle_number));
    Handle stats_pred = _atomspace->add_node(PREDICATE_NODE, "cycle_stats");
    Handle list = _atomspace->add_link(LIST_LINK, HandleSeq{
        cycle_node,
        _atomspace->add_node(NUMBER_NODE, std::to_string(stats.percepts_processed)),
        _atomspace->add_node(NUMBER_NODE, std::to_string(stats.conclusions)),
        _atomspace->add_node(NUMBER_NODE, std::to_string(stats.tasks_processed))
    });
    Handle eval = _atomspace->add_link(EVALUATION_LINK, HandleSeq{stats_pred, list});
    SimpleTruthValue::setTV(eval, 1.0, 0.9);
    (void)_cycle_root;
}

Handle CognitiveLoop::encodePercept(const Percept& p)
{
    Handle src = _atomspace->add_node(CONCEPT_NODE, "PerceptSource:" + p.source);
    Handle content = _atomspace->add_node(CONCEPT_NODE, "Percept:" + p.content);
    Handle modality = _atomspace->add_node(CONCEPT_NODE, "Modality:" + p.modality);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "perceived");
    Handle list = _atomspace->add_link(LIST_LINK, HandleSeq{src, content, modality});
    Handle eval = _atomspace->add_link(EVALUATION_LINK, HandleSeq{pred, list});
    SimpleTruthValue::setTV(eval, std::clamp(p.salience, 0.0, 1.0), 0.9);
    return eval;
}

Handle CognitiveLoop::encodeConclusion(const std::string& name, double strength, double confidence)
{
    Handle c = _atomspace->add_node(CONCEPT_NODE, "Conclusion:" + name);
    SimpleTruthValue::setTV(c, strength, confidence);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "concluded");
    Handle eval = _atomspace->add_link(EVALUATION_LINK, HandleSeq{pred, c});
    SimpleTruthValue::setTV(eval, strength, confidence);
    return eval;
}
