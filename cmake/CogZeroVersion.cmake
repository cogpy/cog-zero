# CogZeroVersion.cmake
#
# Single source of truth for the product version.
# Reads the top-level VERSION file (or falls back to git describe / default).
#
# Sets:
#   COGZERO_VERSION          full version string (e.g. 0.3.0)
#   COGZERO_VERSION_MAJOR
#   COGZERO_VERSION_MINOR
#   COGZERO_VERSION_PATCH
#   COGZERO_SOVERSION        ABI major (major component only)
#
# Usage (before project() is preferred when possible):
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
#   include(CogZeroVersion)
#   project(foo VERSION ${COGZERO_VERSION} ...)

if(DEFINED COGZERO_VERSION AND COGZERO_VERSION)
    # Already set by a parent project
else()
    set(_cogzero_version_file "")
    if(DEFINED COG0_REPO_ROOT AND EXISTS "${COG0_REPO_ROOT}/VERSION")
        set(_cogzero_version_file "${COG0_REPO_ROOT}/VERSION")
    elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../VERSION")
        get_filename_component(_cogzero_version_file
            "${CMAKE_CURRENT_LIST_DIR}/../VERSION" ABSOLUTE)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/VERSION")
        set(_cogzero_version_file "${CMAKE_SOURCE_DIR}/VERSION")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/../VERSION")
        get_filename_component(_cogzero_version_file
            "${CMAKE_SOURCE_DIR}/../VERSION" ABSOLUTE)
    endif()

    if(_cogzero_version_file AND EXISTS "${_cogzero_version_file}")
        file(READ "${_cogzero_version_file}" _cogzero_version_raw)
        string(STRIP "${_cogzero_version_raw}" COGZERO_VERSION)
    else()
        set(COGZERO_VERSION "0.3.0")
        message(STATUS "CogZeroVersion: VERSION file not found; defaulting to ${COGZERO_VERSION}")
    endif()
endif()

if(NOT COGZERO_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
    message(FATAL_ERROR "CogZeroVersion: invalid version '${COGZERO_VERSION}' (expected MAJOR.MINOR.PATCH)")
endif()

string(REPLACE "." ";" _cogzero_version_parts "${COGZERO_VERSION}")
list(GET _cogzero_version_parts 0 COGZERO_VERSION_MAJOR)
list(GET _cogzero_version_parts 1 COGZERO_VERSION_MINOR)
list(GET _cogzero_version_parts 2 COGZERO_VERSION_PATCH)

# SOVERSION tracks major ABI breaks only (semver major).
set(COGZERO_SOVERSION "${COGZERO_VERSION_MAJOR}")

unset(_cogzero_version_file)
unset(_cogzero_version_raw)
unset(_cogzero_version_parts)
