/*
 * cog0_atomspace_bridge.cpp
 *
 * Implementation of the bridge between cog0 standalone and OpenCog AtomSpace.
 * Uses the cog0 C API internally and connects to CogServer via telnet protocol.
 */
#include "cog0_atomspace_bridge.h"
#include "cog0_capi.h"
#include "AtomStore.h"
#include "Agent.h"

#include <cstring>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Helper: extract all quoted strings from an s-expression
static std::vector<std::string> extractQuoted(const std::string& s) {
    std::vector<std::string> result;
    size_t pos = 0;
    while (true) {
        pos = s.find('"', pos);
        if (pos == std::string::npos) break;
        size_t end = s.find('"', pos + 1);
        if (end == std::string::npos) break;
        result.push_back(s.substr(pos + 1, end - pos - 1));
        pos = end + 1;
    }
    return result;
}

// Helper: lowercase
static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

// Helper: strip leading articles
static std::string stripArt(const std::string& s) {
    std::string lo = toLower(s);
    if (lo.size() > 4 && lo.substr(0, 4) == "the ") return s.substr(4);
    if (lo.size() > 3 && lo.substr(0, 3) == "an ") return s.substr(3);
    if (lo.size() > 2 && lo.substr(0, 2) == "a ") return s.substr(2);
    return s;
}

class Bridge {
public:
    cog0::Agent agent;

    Bridge() : agent(cog0::AgentConfig{.name = "cog0-bridge"}) {}

    std::string nlToAtomese(const std::string& text) {
        std::string t = stripArt(text);
        std::string tl = toLower(t);

        // X is a/an Y
        size_t p1 = tl.find(" is a ");
        if (p1 == std::string::npos) p1 = tl.find(" is an ");
        if (p1 != std::string::npos) {
            std::string subj = t.substr(0, p1);
            size_t skip = (tl[p1+5] == 'n') ? 7 : 6;
            std::string obj = stripArt(t.substr(p1 + skip));
            return "(InheritanceLink (ConceptNode \"" + subj + "\") (ConceptNode \"" + obj + "\"))";
        }

        // X is like / resembles Y
        size_t p2 = tl.find(" is like ");
        if (p2 != std::string::npos) {
            std::string subj = t.substr(0, p2);
            std::string obj = stripArt(t.substr(p2 + 9));
            return "(SimilarityLink (ConceptNode \"" + subj + "\") (ConceptNode \"" + obj + "\"))";
        }
        p2 = tl.find(" resembles ");
        if (p2 != std::string::npos) {
            std::string subj = t.substr(0, p2);
            std::string obj = stripArt(t.substr(p2 + 11));
            return "(SimilarityLink (ConceptNode \"" + subj + "\") (ConceptNode \"" + obj + "\"))";
        }

        // X has/have Y
        size_t p3 = tl.find(" has ");
        if (p3 == std::string::npos) p3 = tl.find(" have ");
        if (p3 != std::string::npos) {
            std::string subj = t.substr(0, p3);
            size_t skip = (tl[p3+3] == 's') ? 5 : 6;
            std::string obj = stripArt(t.substr(p3 + skip));
            return "(EvaluationLink (PredicateNode \"has\") (ListLink (ConceptNode \"" + subj + "\") (ConceptNode \"" + obj + "\")))";
        }

        // X belongs to Y
        size_t p4 = tl.find(" belongs to ");
        if (p4 != std::string::npos) {
            std::string subj = t.substr(0, p4);
            std::string obj = stripArt(t.substr(p4 + 12));
            return "(MemberLink (ConceptNode \"" + subj + "\") (ConceptNode \"" + obj + "\"))";
        }

        // X verb Y (fallback SVO)
        size_t sp1 = t.find(' ');
        if (sp1 != std::string::npos) {
            size_t sp2 = t.find(' ', sp1 + 1);
            if (sp2 != std::string::npos) {
                std::string subj = t.substr(0, sp1);
                std::string verb = t.substr(sp1 + 1, sp2 - sp1 - 1);
                std::string obj = stripArt(t.substr(sp2 + 1));
                return "(EvaluationLink (PredicateNode \"" + verb + "\") (ListLink (ConceptNode \"" + subj + "\") (ConceptNode \"" + obj + "\")))";
            }
        }

        return "(ConceptNode \"" + text + "\")";
    }

    std::string atomeseToNl(const std::string& atomese) {
        std::vector<std::string> names = extractQuoted(atomese);
        if (atomese.find("InheritanceLink") != std::string::npos && names.size() >= 2) {
            return names[0] + " is a " + names[1] + ".";
        }
        if (atomese.find("SimilarityLink") != std::string::npos && names.size() >= 2) {
            return names[0] + " is similar to " + names[1] + ".";
        }
        if (atomese.find("EvaluationLink") != std::string::npos && names.size() >= 3) {
            return names[1] + " " + names[0] + " " + names[2] + ".";
        }
        if (atomese.find("MemberLink") != std::string::npos && names.size() >= 2) {
            return names[0] + " belongs to " + names[1] + ".";
        }
        return atomese;
    }

    int syncToCogServer(const char* host, int port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return -1;

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host, &addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            return -1;
        }

        // Send each atom as a Scheme expression to the CogServer
        auto atoms = agent.atomStore().getByType(cog0::AtomType::CONCEPT);
        int count = 0;
        for (auto& atom : atoms) {
            std::string cmd = "(ConceptNode \"" + atom->name() + "\")\n";
            ::send(sock, cmd.c_str(), cmd.size(), 0);
            count++;
        }

        close(sock);
        return count;
    }
};

extern "C" {

cog0_bridge_t cog0_bridge_create(void) {
    return new Bridge();
}

void cog0_bridge_destroy(cog0_bridge_t bridge) {
    delete static_cast<Bridge*>(bridge);
}

char* cog0_bridge_nl_to_atomese(cog0_bridge_t bridge, const char* text) {
    auto b = static_cast<Bridge*>(bridge);
    std::string result = b->nlToAtomese(text);
    return strdup(result.c_str());
}

char* cog0_bridge_atomese_to_nl(cog0_bridge_t bridge, const char* atomese) {
    auto b = static_cast<Bridge*>(bridge);
    std::string result = b->atomeseToNl(atomese);
    return strdup(result.c_str());
}

void cog0_bridge_percept_and_run(cog0_bridge_t bridge, const char* text, int cycles) {
    auto b = static_cast<Bridge*>(bridge);
    b->agent.addPercept("bridge", text, 0.8);
    for (int i = 0; i < cycles; i++) {
        b->agent.cognitiveLoop().runSingleCycle();
    }
}

int cog0_bridge_sync_to_cogserver(cog0_bridge_t bridge, const char* host, int port) {
    auto b = static_cast<Bridge*>(bridge);
    return b->syncToCogServer(host, port);
}

int cog0_bridge_atom_count(cog0_bridge_t bridge) {
    auto b = static_cast<Bridge*>(bridge);
    return (int)b->agent.atomStore().size();
}

char* cog0_bridge_status(cog0_bridge_t bridge) {
    auto b = static_cast<Bridge*>(bridge);
    std::ostringstream ss;
    ss << "cog0-bridge v0.1.0\n";
    ss << "Atoms: " << b->agent.atomStore().size() << "\n";
    ss << "Cycles: " << b->agent.cognitiveLoop().cycleCount() << "\n";
    return strdup(ss.str().c_str());
}

const char* cog0_bridge_version(void) {
    return "0.1.0";
}

} // extern "C"
