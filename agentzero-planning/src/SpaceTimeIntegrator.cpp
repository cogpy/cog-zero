/*
 * opencog/agentzero/planning/SpaceTimeIntegrator.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/planning/SpaceTimeIntegrator.h"

using namespace opencog;
using namespace opencog::agentzero::planning;

namespace {

#ifdef HAVE_SPACETIME
inline double pointDistance(const Point3& a, const Point3& b) {
    return a.distance(b);
}
#else
inline double pointDistance(const Point3& a, const Point3& b) {
    return a.distance(b);
}
inline Point3 lerp(const Point3& a, const Point3& b, double t) {
    return a + (b - a) * t;
}
#endif

} // namespace

SpaceTimeIntegrator::SpaceTimeIntegrator(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _current_time(std::chrono::system_clock::now())
{
    if (!_atomspace)
        throw std::runtime_error("SpaceTimeIntegrator requires a valid AtomSpace");
}

SpaceTimeIntegrator::~SpaceTimeIntegrator() = default;

bool SpaceTimeIntegrator::initialize()
{
    if (_initialized) return true;
    createContextAtoms();

#ifdef HAVE_SPACETIME
    _spacetime_map = std::make_unique<TimeOctomap<Handle>>(
        _config.time_units,
        _config.spatial_resolution,
        std::chrono::milliseconds(_config.time_resolution));
    logger().info() << "[SpaceTimeIntegrator] Native spacetime TimeOctomap enabled";
#else
    logger().info() << "[SpaceTimeIntegrator] Using in-process spacetime fallback";
#endif

    _initialized = true;
    return true;
}

bool SpaceTimeIntegrator::shutdown()
{
    _location_occupancy.clear();
    _atom_timeline.clear();
#ifdef HAVE_SPACETIME
    _spacetime_map.reset();
#endif
    _initialized = false;
    return true;
}

void SpaceTimeIntegrator::createContextAtoms()
{
    _spatial_context = _atomspace->add_node(CONCEPT_NODE, "SpatialPlanningContext");
    _temporal_context = _atomspace->add_node(CONCEPT_NODE, "TemporalSpaceContext");
    _trajectory_context = _atomspace->add_node(CONCEPT_NODE, "TrajectoryContext");
    _spatial_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
}

void SpaceTimeIntegrator::configure(const Configuration& config)
{
    _config = config;
}

bool SpaceTimeIntegrator::isValidLocation(const Point3& location) const
{
#ifdef HAVE_SPACETIME
    (void)location;
    return true;
#else
    return std::isfinite(location.x) && std::isfinite(location.y) &&
           std::isfinite(location.z);
#endif
}

bool SpaceTimeIntegrator::insertAtomAtLocation(
    const Handle& atom,
    const Point3& location,
    const std::chrono::system_clock::time_point& time)
{
    if (!_initialized) initialize();
    if (!atom || !isValidLocation(location)) return false;

#ifdef HAVE_SPACETIME
    if (_spacetime_map)
        _spacetime_map->insert_atom(location, atom);
#else
    (void)0;
#endif

    _location_occupancy[location].push_back(atom);
    _atom_timeline[atom].emplace_back(time, location);
    createSpatialTemporalAtom(atom, location, time);
    return true;
}

bool SpaceTimeIntegrator::insertAtomAtCurrentTime(const Handle& atom,
                                                  const Point3& location)
{
    return insertAtomAtLocation(atom, location, _current_time);
}

bool SpaceTimeIntegrator::removeAtomFromLocation(
    const Handle& atom,
    const Point3& location,
    const std::chrono::system_clock::time_point& time)
{
    auto it = _location_occupancy.find(location);
    if (it == _location_occupancy.end()) return false;
    auto& vec = it->second;
    auto pos = std::find(vec.begin(), vec.end(), atom);
    if (pos == vec.end()) return false;
    vec.erase(pos);
    if (vec.empty()) _location_occupancy.erase(it);

    auto tit = _atom_timeline.find(atom);
    if (tit != _atom_timeline.end()) {
        auto& tl = tit->second;
        tl.erase(std::remove_if(tl.begin(), tl.end(),
            [&](const auto& p) {
                return p.second == location && p.first == time;
            }), tl.end());
    }
    return true;
}

std::vector<Handle> SpaceTimeIntegrator::getAtomsAtLocation(
    const Point3& location,
    const std::chrono::system_clock::time_point& time) const
{
    (void)time;
    auto it = _location_occupancy.find(location);
    if (it == _location_occupancy.end()) return {};
    return it->second;
}

std::vector<Point3> SpaceTimeIntegrator::getAtomLocations(
    const Handle& atom,
    const std::chrono::system_clock::time_point& time) const
{
    std::vector<Point3> out;
    auto it = _atom_timeline.find(atom);
    if (it == _atom_timeline.end()) return out;
    for (const auto& p : it->second) {
        // nearest time sample
        if (p.first <= time || out.empty())
            out.push_back(p.second);
    }
    if (out.size() > 1) {
        // keep last only for simplicity
        Point3 last = out.back();
        out.clear();
        out.push_back(last);
    }
    return out;
}

double SpaceTimeIntegrator::getDistanceBetween(
    const Handle& atom1,
    const Handle& atom2,
    const std::chrono::system_clock::time_point& time) const
{
    auto a = getAtomLocations(atom1, time);
    auto b = getAtomLocations(atom2, time);
    if (a.empty() || b.empty()) return -1.0;
    return pointDistance(a.front(), b.front());
}

bool SpaceTimeIntegrator::validateSpatialConstraints(
    const std::vector<SpatialConstraint>& constraints) const
{
    for (const auto& c : constraints) {
        if (!isValidLocation(c.location)) return false;
        // Exact-key occupancy check; tolerance reserved for future nearest-neighbor
        (void)c.tolerance;
        auto atoms = getAtomsAtLocation(c.location, c.start_time);
        for (const Handle& h : atoms) {
            if (h != c.atom)
                return false;
        }
    }
    return true;
}

bool SpaceTimeIntegrator::checkLocationAvailability(
    const Point3& location,
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) const
{
    (void)start_time;
    (void)end_time;
    auto it = _location_occupancy.find(location);
    if (it == _location_occupancy.end()) return true;
    return it->second.empty();
}

bool SpaceTimeIntegrator::planTrajectory(
    const Handle& atom,
    const Point3& start_location,
    const Point3& goal_location,
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time,
    Trajectory& result_trajectory)
{
    if (!_initialized) initialize();
    if (!_config.enable_trajectory_planning) return false;
    if (!isValidLocation(start_location) || !isValidLocation(goal_location))
        return false;
    if (end_time <= start_time) return false;

    result_trajectory.clear();
    const int steps = std::max(2, static_cast<int>(
        pointDistance(start_location, goal_location) /
        std::max(_config.spatial_resolution, 1e-6)));

    auto total = end_time - start_time;
    for (int i = 0; i <= steps; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(steps);
#ifdef HAVE_SPACETIME
        Point3 p(
            start_location.x() + (goal_location.x() - start_location.x()) * t,
            start_location.y() + (goal_location.y() - start_location.y()) * t,
            start_location.z() + (goal_location.z() - start_location.z()) * t);
#else
        Point3 p = lerp(start_location, goal_location, t);
#endif
        TrajectoryPoint tp;
        tp.location = p;
        tp.time = start_time + std::chrono::duration_cast<std::chrono::system_clock::duration>(
            total * t);
        tp.associated_atom = atom;
        result_trajectory.push_back(tp);
    }

    // Record endpoints
    insertAtomAtLocation(atom, start_location, start_time);
    insertAtomAtLocation(atom, goal_location, end_time);
    return validateTrajectory(result_trajectory);
}

bool SpaceTimeIntegrator::validateTrajectory(const Trajectory& trajectory) const
{
    if (trajectory.size() < 2) return false;
    for (size_t i = 1; i < trajectory.size(); ++i) {
        if (trajectory[i].time < trajectory[i - 1].time) return false;
        if (!isValidLocation(trajectory[i].location)) return false;
    }
    return true;
}

SpaceTimeIntegrator::TemporalPlanningResult
SpaceTimeIntegrator::findOptimalTimeWindow(
    const Handle& action_atom,
    const std::vector<SpatialConstraint>& spatial_requirements,
    const std::chrono::system_clock::time_point& earliest_start,
    const std::chrono::system_clock::time_point& latest_end)
{
    TemporalPlanningResult result;
    result.feasible = false;
    result.confidence_score = 0.0;
    if (latest_end <= earliest_start) return result;

    auto slot = _config.time_resolution;
    if (slot.count() <= 0) slot = std::chrono::milliseconds(100);

    for (auto t = earliest_start; t + slot <= latest_end; t += slot) {
        bool free = true;
        for (const auto& req : spatial_requirements) {
            if (!checkLocationAvailability(req.location, t, t + slot)) {
                free = false;
                break;
            }
        }
        if (free) {
            result.optimal_start_time = t;
            result.optimal_end_time = t + slot;
            result.required_constraints = spatial_requirements;
            result.confidence_score = 0.9;
            result.feasible = true;
            // reserve
            for (const auto& req : spatial_requirements) {
                // non-const cast path: insert via copy of this
                const_cast<SpaceTimeIntegrator*>(this)->insertAtomAtLocation(
                    action_atom != Handle::UNDEFINED ? action_atom : req.atom,
                    req.location, t);
            }
            return result;
        }
    }
    return result;
}

void SpaceTimeIntegrator::stepTimeUnit()
{
    _current_time += _config.time_resolution;
}

void SpaceTimeIntegrator::setCurrentTime(
    const std::chrono::system_clock::time_point& time)
{
    _current_time = time;
}

Handle SpaceTimeIntegrator::createSpatialTemporalAtom(
    const Handle& atom,
    const Point3& location,
    const std::chrono::system_clock::time_point& time)
{
#ifdef HAVE_SPACETIME
    std::string loc = std::to_string(location.x()) + "," +
                      std::to_string(location.y()) + "," +
                      std::to_string(location.z());
#else
    std::string loc = std::to_string(location.x) + "," +
                      std::to_string(location.y) + "," +
                      std::to_string(location.z);
#endif
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch()).count();
    Handle loc_node = _atomspace->add_node(CONCEPT_NODE, "Loc_" + loc);
    Handle time_node = _atomspace->add_node(NUMBER_NODE, std::to_string(ms));
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "at-location-time");
    Handle eval = _atomspace->add_link(EVALUATION_LINK, {
        pred,
        _atomspace->add_link(LIST_LINK, {atom, loc_node, time_node})
    });
    _atomspace->add_link(MEMBER_LINK, {eval, _spatial_context});
    return eval;
}

std::string SpaceTimeIntegrator::getStatusInfo() const
{
    std::ostringstream oss;
    oss << "{\"initialized\":" << (_initialized ? "true" : "false")
        << ",\"native_spacetime\":" << (hasNativeSpacetime() ? "true" : "false")
        << ",\"occupancy\":" << _location_occupancy.size()
        << ",\"tracked_atoms\":" << _atom_timeline.size()
        << "}";
    return oss.str();
}
