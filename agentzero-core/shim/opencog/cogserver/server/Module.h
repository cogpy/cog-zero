/*
 * Minimal CogServer Module base for agentzero-core shim builds.
 */
#ifndef _AGENTZERO_SHIM_MODULE_H
#define _AGENTZERO_SHIM_MODULE_H

#include <memory>
#include <stdexcept>
#include <string>

namespace opencog {

class CogServer;
class AtomSpace;
using AtomSpacePtr = std::shared_ptr<AtomSpace>;

class Module {
public:
    Module() : _cogserver_ptr(nullptr) {}
    explicit Module(CogServer& cs) : _cogserver_ptr(&cs) {}
    virtual ~Module() = default;

    virtual void init() = 0;
    virtual bool config(const char*) { return true; }
    virtual const char* id() = 0;

protected:
    CogServer* _cogserver_ptr;
    // Compatibility alias used by some OpenCog module code
    CogServer& _cogserver() {
        if (!_cogserver_ptr) throw std::runtime_error("No CogServer bound to module");
        return *_cogserver_ptr;
    }
};

#ifndef DECLARE_MODULE
#define DECLARE_MODULE(CLASSNAME) /* shim: no dynamic module factory */
#endif

} // namespace opencog

#endif
