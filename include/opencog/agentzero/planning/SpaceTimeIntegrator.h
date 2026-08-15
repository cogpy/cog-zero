/*
 * opencog/agentzero/planning/SpaceTimeIntegrator.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Temporal/spatial planning bridge (Phase 4).
 * Uses OpenCog spacetime when HAVE_SPACETIME is defined; otherwise a
 * lightweight in-process timeline + 3D occupancy map.
 */

#ifndef _OPENCOG_AGENTZERO_PLANNING_SPACETIME_INTEGRATOR_H
#define _OPENCOG_AGENTZERO_PLANNING_SPACETIME_INTEGRATOR_H

#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#ifdef HAVE_SPACETIME
#include <opencog/spacetime/octomap/TimeOctomap.h>
#endif

namespace opencog {
namespace agentzero {
namespace planning {

/** Simple 3-D point used when octomap is unavailable. */
struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    double distance(const Vec3& o) const {
        const double dx = x - o.x, dy = y - o.y, dz = z - o.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }

    bool operator<(const Vec3& o) const {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return z < o.z;
    }
    bool operator==(const Vec3& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

#ifdef HAVE_SPACETIME
using Point3 = octomap::point3d;
#else
using Point3 = Vec3;
#endif

/**
 * SpaceTimeIntegrator — trajectory planning and optimal time-window search.
 */
class SpaceTimeIntegrator
{
public:
    struct Configuration {
        double spatial_resolution{0.1};
        std::chrono::milliseconds time_resolution{100};
        unsigned int time_units{1000};
        bool enable_spatial_constraints{true};
        bool enable_trajectory_planning{true};
        bool enable_timeline_reasoning{true};
    };

    struct SpatialConstraint {
        Handle atom;
        Point3 location;
        double tolerance{0.1};
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
    };

    struct TrajectoryPoint {
        Point3 location;
        std::chrono::system_clock::time_point time;
        Handle associated_atom;
    };

    using Trajectory = std::vector<TrajectoryPoint>;

    struct TemporalPlanningResult {
        std::chrono::system_clock::time_point optimal_start_time;
        std::chrono::system_clock::time_point optimal_end_time;
        std::vector<SpatialConstraint> required_constraints;
        double confidence_score{0.0};
        bool feasible{false};
    };

    explicit SpaceTimeIntegrator(AtomSpacePtr atomspace);
    ~SpaceTimeIntegrator();

    bool initialize();
    bool shutdown();
    bool isInitialized() const { return _initialized; }

    void configure(const Configuration& config);
    const Configuration& getConfiguration() const { return _config; }

    bool insertAtomAtLocation(const Handle& atom,
                              const Point3& location,
                              const std::chrono::system_clock::time_point& time);
    bool insertAtomAtCurrentTime(const Handle& atom, const Point3& location);
    bool removeAtomFromLocation(const Handle& atom,
                                const Point3& location,
                                const std::chrono::system_clock::time_point& time);

    std::vector<Handle> getAtomsAtLocation(
        const Point3& location,
        const std::chrono::system_clock::time_point& time) const;
    std::vector<Point3> getAtomLocations(
        const Handle& atom,
        const std::chrono::system_clock::time_point& time) const;

    double getDistanceBetween(const Handle& atom1,
                              const Handle& atom2,
                              const std::chrono::system_clock::time_point& time) const;

    bool validateSpatialConstraints(const std::vector<SpatialConstraint>& constraints) const;
    bool checkLocationAvailability(const Point3& location,
                                   const std::chrono::system_clock::time_point& start_time,
                                   const std::chrono::system_clock::time_point& end_time) const;

    bool planTrajectory(const Handle& atom,
                        const Point3& start_location,
                        const Point3& goal_location,
                        const std::chrono::system_clock::time_point& start_time,
                        const std::chrono::system_clock::time_point& end_time,
                        Trajectory& result_trajectory);
    bool validateTrajectory(const Trajectory& trajectory) const;

    TemporalPlanningResult findOptimalTimeWindow(
        const Handle& action_atom,
        const std::vector<SpatialConstraint>& spatial_requirements,
        const std::chrono::system_clock::time_point& earliest_start,
        const std::chrono::system_clock::time_point& latest_end);

    void stepTimeUnit();
    void setCurrentTime(const std::chrono::system_clock::time_point& time);
    std::chrono::system_clock::time_point getCurrentTime() const { return _current_time; }

    Handle createSpatialTemporalAtom(const Handle& atom,
                                     const Point3& location,
                                     const std::chrono::system_clock::time_point& time);

    std::string getStatusInfo() const;
    size_t getOccupancyCount() const { return _location_occupancy.size(); }

    Handle getSpatialContext() const { return _spatial_context; }
    Handle getTemporalContext() const { return _temporal_context; }
    Handle getTrajectoryContext() const { return _trajectory_context; }

    bool hasNativeSpacetime() const {
#ifdef HAVE_SPACETIME
        return true;
#else
        return false;
#endif
    }

private:
    AtomSpacePtr _atomspace;
    bool _initialized{false};
    Configuration _config;

#ifdef HAVE_SPACETIME
    std::unique_ptr<TimeOctomap<Handle>> _spacetime_map;
#endif

    Handle _spatial_context;
    Handle _temporal_context;
    Handle _trajectory_context;

    std::chrono::system_clock::time_point _current_time;
    std::map<Point3, std::vector<Handle>> _location_occupancy;
    // atom → list of (time, location)
    std::map<Handle, std::vector<std::pair<std::chrono::system_clock::time_point, Point3>>> _atom_timeline;

    void createContextAtoms();
    bool isValidLocation(const Point3& location) const;
};

} // namespace planning
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_PLANNING_SPACETIME_INTEGRATOR_H
