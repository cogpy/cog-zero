/*
 * agentzero-perception/src/AttentionManager.cpp
 *
 * ECAN-inspired STI allocation, decay, focus, and spreading.
 */
#include "opencog/agentzero/AttentionManager.h"

#include <opencog/util/Logger.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

using namespace opencog;
using namespace opencog::agentzero;

AttentionManager::AttentionManager(AtomSpacePtr atomspace, const AttentionConfig& config)
    : _atomspace(std::move(atomspace))
    , _config(config)
{
    if (!_atomspace) {
        throw std::invalid_argument("AttentionManager requires a valid AtomSpace");
    }
    // Normalize config bounds
    if (_config.decay_rate < 0.0) _config.decay_rate = 0.0;
    if (_config.decay_rate > 1.0) _config.decay_rate = 1.0;
    if (_config.min_sti < 0.0) _config.min_sti = 0.0;
    if (_config.max_sti < _config.min_sti) _config.max_sti = _config.min_sti;
}

AttentionManager::~AttentionManager() = default;

// static
double AttentionManager::clamp01(double v)
{
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

void AttentionManager::setSTIUnlocked(const Handle& atom, double sti)
{
    if (!std::isfinite(sti)) {
        return; // ignore NaN/Inf — do not poison the attention map
    }
    if (sti < _config.min_sti) sti = _config.min_sti;
    if (sti > _config.max_sti) sti = _config.max_sti;
    _sti[atom] = sti;

    // Best-effort mirror onto Atom STI (shim uses short).
    // Always project into a fixed 0..1000 display range from the normalized
    // position within [min_sti, max_sti] so large max_sti cannot overflow short.
    if (atom) {
        double span = _config.max_sti - _config.min_sti;
        double norm = (span > 1e-12) ? ((sti - _config.min_sti) / span) : 0.0;
        if (norm < 0.0) norm = 0.0;
        if (norm > 1.0) norm = 1.0;
        long scaled = std::lround(norm * 1000.0);
        if (scaled < 0) scaled = 0;
        if (scaled > 1000) scaled = 1000;
        atom->setSTI(static_cast<short>(scaled));
    }
}

double AttentionManager::allocateAttention(const Handle& atom, double salience)
{
    if (!atom) {
        return 0.0;
    }
    if (!std::isfinite(salience)) {
        return 0.0;
    }

    std::lock_guard<std::mutex> lock(_mu);
    double s = clamp01(salience);
    // STI = base * salience, blended toward max for high salience
    double sti = _config.base_sti * s + (1.0 - _config.base_sti) * s * s;
    sti = std::min(_config.max_sti, std::max(_config.min_sti, sti));
    setSTIUnlocked(atom, sti);
    ++_allocations;
    return sti;
}

void AttentionManager::updateAttention(const Handle& atom, double sti)
{
    if (!atom) return;
    if (!std::isfinite(sti)) return;
    std::lock_guard<std::mutex> lock(_mu);
    setSTIUnlocked(atom, sti);
}

double AttentionManager::getSTI(const Handle& atom) const
{
    if (!atom) return 0.0;
    std::lock_guard<std::mutex> lock(_mu);
    auto it = _sti.find(atom);
    if (it == _sti.end()) return 0.0;
    return it->second;
}

size_t AttentionManager::trackedAtomCount() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _sti.size();
}

void AttentionManager::decayAttention()
{
    std::lock_guard<std::mutex> lock(_mu);
    const double keep = 1.0 - _config.decay_rate;
    std::vector<Handle> drop;
    for (auto& kv : _sti) {
        if (!std::isfinite(kv.second)) {
            drop.push_back(kv.first);
            continue;
        }
        double next = kv.second * keep;
        if (!std::isfinite(next) || next <= _config.min_sti + 1e-12) {
            drop.push_back(kv.first);
        } else {
            setSTIUnlocked(kv.first, next);
        }
    }
    for (const auto& h : drop) {
        _sti.erase(h);
        if (h) h->setSTI(0);
    }
    ++_decay_cycles;
}

HandleSeq AttentionManager::getAttentionFocus() const
{
    std::lock_guard<std::mutex> lock(_mu);
    HandleSeq focus;
    for (const auto& kv : _sti) {
        if (kv.second >= _config.focus_boundary) {
            focus.push_back(kv.first);
        }
    }
    return focus;
}

bool AttentionManager::isInAttentionFocus(const Handle& atom) const
{
    if (!atom) return false;
    std::lock_guard<std::mutex> lock(_mu);
    auto it = _sti.find(atom);
    if (it == _sti.end()) return false;
    return it->second >= _config.focus_boundary;
}

SalienceScore AttentionManager::calculateSalience(const SensoryInput& input)
{
    SalienceScore score;

    // Signal quality from confidence and finite data density
    double conf = clamp01(input.confidence);
    double finite_frac = 1.0;
    if (!input.data.empty()) {
        size_t good = 0;
        for (double v : input.data) {
            if (std::isfinite(v)) ++good;
        }
        finite_frac = static_cast<double>(good) / static_cast<double>(input.data.size());
    }
    score.signal_quality = clamp01(0.7 * conf + 0.3 * finite_frac);

    // Novelty decays with repeated modality observations
    std::string key = input.sensor_type + "|" + input.modality;
    size_t seen = 0;
    {
        std::lock_guard<std::mutex> lock(_mu);
        seen = _modality_seen[key]++;
    }
    score.novelty = clamp01(1.0 / (1.0 + static_cast<double>(seen)));

    score.overall = clamp01(0.6 * score.signal_quality + 0.4 * score.novelty);
    return score;
}

void AttentionManager::spreadAttention(const Handle& source, double amount)
{
    if (!source) return;
    if (amount <= 0.0) return;

    HandleSeq targets;
    if (source->is_link()) {
        targets = source->getOutgoingSet();
    } else {
        // Nothing to spread to for a bare node without graph walk support
        return;
    }
    if (targets.empty()) return;

    double share = amount / static_cast<double>(targets.size());
    std::lock_guard<std::mutex> lock(_mu);
    for (const auto& t : targets) {
        if (!t) continue;
        double cur = 0.0;
        auto it = _sti.find(t);
        if (it != _sti.end()) cur = it->second;
        setSTIUnlocked(t, cur + share);
    }
}

std::string AttentionManager::getStats() const
{
    std::lock_guard<std::mutex> lock(_mu);
    size_t focus_size = 0;
    for (const auto& kv : _sti) {
        if (kv.second >= _config.focus_boundary) ++focus_size;
    }
    std::ostringstream oss;
    oss << "{\"allocations\":" << _allocations
        << ",\"decay_cycles\":" << _decay_cycles
        << ",\"tracked_atoms\":" << _sti.size()
        << ",\"focus_size\":" << focus_size
        << "}";
    return oss.str();
}

void AttentionManager::reset()
{
    std::lock_guard<std::mutex> lock(_mu);
    _sti.clear();
    _modality_seen.clear();
    _allocations = 0;
    _decay_cycles = 0;
}
