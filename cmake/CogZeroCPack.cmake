# CogZeroCPack.cmake
#
# Shared CPack configuration for cog0 (standalone) and cog-zero (monorepo).
#
# Expected variables (set by caller before include):
#   COGZERO_VERSION          product version
#   COGZERO_CPACK_PACKAGE_NAME   "cog0" or "cog-zero"
#   COGZERO_CPACK_SUMMARY        short description
#
# Optional:
#   COGZERO_CPACK_CONTACT
#   COGZERO_REPO_ROOT            path to LICENSE
#   COGZERO_ENABLE_CPACK         if OFF, skip include(CPack)

if(DEFINED COGZERO_ENABLE_CPACK AND NOT COGZERO_ENABLE_CPACK)
    return()
endif()

if(NOT DEFINED COGZERO_VERSION OR NOT COGZERO_VERSION)
    message(FATAL_ERROR "CogZeroCPack: COGZERO_VERSION is not set")
endif()

if(NOT DEFINED COGZERO_CPACK_PACKAGE_NAME)
    set(COGZERO_CPACK_PACKAGE_NAME "cog0")
endif()

if(NOT DEFINED COGZERO_CPACK_SUMMARY)
    set(COGZERO_CPACK_SUMMARY "cog0 — Standalone Agent-Zero C++ runtime")
endif()

if(NOT DEFINED COGZERO_CPACK_CONTACT)
    set(COGZERO_CPACK_CONTACT "https://github.com/cogpy/cog-zero")
endif()

if(NOT DEFINED COGZERO_REPO_ROOT)
    if(DEFINED COG0_REPO_ROOT)
        set(COGZERO_REPO_ROOT "${COG0_REPO_ROOT}")
    else()
        set(COGZERO_REPO_ROOT "${CMAKE_SOURCE_DIR}")
    endif()
endif()

set(CPACK_PACKAGE_NAME "${COGZERO_CPACK_PACKAGE_NAME}")
set(CPACK_PACKAGE_VENDOR "cogpy")
set(CPACK_PACKAGE_CONTACT "${COGZERO_CPACK_CONTACT}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${COGZERO_CPACK_SUMMARY}")
set(CPACK_PACKAGE_VERSION "${COGZERO_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${COGZERO_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${COGZERO_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${COGZERO_VERSION_PATCH}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/cogpy/cog-zero")

if(EXISTS "${COGZERO_REPO_ROOT}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${COGZERO_REPO_ROOT}/LICENSE")
endif()
if(EXISTS "${COGZERO_REPO_ROOT}/README.md")
    set(CPACK_RESOURCE_FILE_README "${COGZERO_REPO_ROOT}/README.md")
endif()

# System / arch for portable archive names
string(TOLOWER "${CMAKE_SYSTEM_NAME}" _cpack_sys)
if(_cpack_sys STREQUAL "darwin")
    set(_cpack_sys "macos")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(_cpack_arch "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(_cpack_arch "arm64")
else()
    set(_cpack_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${_cpack_sys}-${_cpack_arch}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-source")

set(CPACK_GENERATOR "TGZ;ZIP")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND CPACK_GENERATOR "DEB")
    # RPM is optional; only add when rpmbuild is present.
    find_program(_cpack_rpmbuild NAMES rpmbuild)
    if(_cpack_rpmbuild)
        list(APPEND CPACK_GENERATOR "RPM")
    endif()
endif()

set(CPACK_STRIP_FILES ON)
set(CPACK_VERBATIM_VARIABLES ON)

# Components
set(CPACK_COMPONENTS_ALL Runtime Development)
if(TARGET cog0_capi OR DEFINED COGZERO_CPACK_INCLUDE_PYTHON)
    list(APPEND CPACK_COMPONENTS_ALL Python)
endif()
if(DEFINED COGZERO_CPACK_INCLUDE_OPENCOG AND COGZERO_CPACK_INCLUDE_OPENCOG)
    list(APPEND CPACK_COMPONENTS_ALL OpenCog)
endif()

set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION
    "cog0 executable and runtime shared libraries")
set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Development")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION
    "Static libraries, headers, CMake config, and pkg-config files")
set(CPACK_COMPONENT_DEVELOPMENT_DEPENDS Runtime)
set(CPACK_COMPONENT_PYTHON_DISPLAY_NAME "Python")
set(CPACK_COMPONENT_PYTHON_DESCRIPTION
    "Python cog0 package and libcog0_capi")
set(CPACK_COMPONENT_OPENCOG_DISPLAY_NAME "OpenCog")
set(CPACK_COMPONENT_OPENCOG_DESCRIPTION
    "OpenCog-integrated agentzero libraries and CogZero CMake package")

# DEB/RPM: one package per component (cog0 / cog0-dev).
# TGZ/ZIP: single archive with all selected components (simpler GitHub Releases).
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)

# Emit packages into the build tree (not the caller's cwd).
if(NOT CPACK_PACKAGE_DIRECTORY)
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

# Debian / RPM package names per component
if(COGZERO_CPACK_PACKAGE_NAME STREQUAL "cog0")
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "cog0")
    set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_NAME "cog0-dev")
    set(CPACK_DEBIAN_PYTHON_PACKAGE_NAME "python3-cog0")
    set(CPACK_RPM_RUNTIME_PACKAGE_NAME "cog0")
    set(CPACK_RPM_DEVELOPMENT_PACKAGE_NAME "cog0-devel")
    set(CPACK_RPM_PYTHON_PACKAGE_NAME "python3-cog0")
else()
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "cog-zero")
    set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_NAME "cog-zero-dev")
    set(CPACK_DEBIAN_PYTHON_PACKAGE_NAME "python3-cog0")
    set(CPACK_DEBIAN_OPENCOG_PACKAGE_NAME "cog-zero-opencog")
    set(CPACK_RPM_RUNTIME_PACKAGE_NAME "cog-zero")
    set(CPACK_RPM_DEVELOPMENT_PACKAGE_NAME "cog-zero-devel")
    set(CPACK_RPM_PYTHON_PACKAGE_NAME "python3-cog0")
    set(CPACK_RPM_OPENCOG_PACKAGE_NAME "cog-zero-opencog")
endif()

set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

# Standalone runtime depends only on libc (+ optional openssl at link time).
set(CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS "libc6")
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_DEPENDS
    "${CPACK_DEBIAN_RUNTIME_PACKAGE_NAME} (= \${binary:Version})")

include(CPack)

unset(_cpack_sys)
unset(_cpack_arch)
unset(_cpack_rpmbuild)
