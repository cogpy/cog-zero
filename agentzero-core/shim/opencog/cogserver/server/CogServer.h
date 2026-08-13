/*
 * Minimal CogServer stub for agentzero-core module registration tests.
 */
#ifndef _AGENTZERO_SHIM_COGSERVER_H
#define _AGENTZERO_SHIM_COGSERVER_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/cogserver/server/Module.h>

namespace opencog {

class CogServer {
public:
    CogServer() : _atomspace(std::make_shared<AtomSpace>()) {}
    explicit CogServer(AtomSpacePtr as) : _atomspace(std::move(as)) {
        if (!_atomspace) _atomspace = std::make_shared<AtomSpace>();
    }

    AtomSpacePtr getAtomSpace() const { return _atomspace; }
    void setAtomSpace(AtomSpacePtr as) { _atomspace = std::move(as); }

    void loadModules() { _modules_loaded = true; }
    bool modulesLoaded() const { return _modules_loaded; }

    // Simple command registry used by tests to verify module commands.
    using CommandHandler = std::function<std::string(const std::string&)>;

    void registerCommand(const std::string& name, CommandHandler handler) {
        _commands[name] = std::move(handler);
    }

    bool hasCommand(const std::string& name) const {
        return _commands.find(name) != _commands.end();
    }

    std::string runCommand(const std::string& name, const std::string& args = "") {
        auto it = _commands.find(name);
        if (it == _commands.end()) return "ERROR: unknown command: " + name;
        return it->second(args);
    }

    std::vector<std::string> listCommands() const {
        std::vector<std::string> names;
        names.reserve(_commands.size());
        for (const auto& kv : _commands) names.push_back(kv.first);
        return names;
    }

private:
    AtomSpacePtr _atomspace;
    bool _modules_loaded = false;
    std::unordered_map<std::string, CommandHandler> _commands;
};

} // namespace opencog

#endif
