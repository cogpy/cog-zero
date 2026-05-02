# opencog/cog0/CMakeLists.txt
#
# Builds the cog0 library for OpenCog integration.
# Requires cogutil and atomspace.
#
# Source files live in the repository's src/ and include/ directories.
# Headers are installed under <prefix>/include/opencog/cog0/.

# -----------------------------------------------------------------------
# Collect sources that integrate with OpenCog (cogutil / atomspace)

SET(COG0_SRC "${CMAKE_SOURCE_DIR}/src")
SET(COG0_INC "${CMAKE_SOURCE_DIR}/include")

SET(COG0_SOURCES
    ${COG0_SRC}/AtomStore.cpp
    ${COG0_SRC}/AtomSpaceEvolver.cpp
    ${COG0_SRC}/TaskManager.cpp
    ${COG0_SRC}/ReasoningEngine.cpp
    ${COG0_SRC}/CognitiveLoop.cpp
    ${COG0_SRC}/Agent.cpp
    ${COG0_SRC}/cog0_capi.cpp
    ${COG0_SRC}/MultiModalSensor.cpp
    ${COG0_SRC}/PerceptualProcessor.cpp
    ${COG0_SRC}/AttentionManager.cpp
    ${COG0_SRC}/TextualSensor.cpp
    ${COG0_SRC}/ActionExecutor.cpp
    ${COG0_SRC}/ActionScheduler.cpp
    ${COG0_SRC}/KnowledgeIntegrator.cpp
    ${COG0_SRC}/EpisodicMemory.cpp
    ${COG0_SRC}/ExperienceManager.cpp
    ${COG0_SRC}/GoalHierarchy.cpp
    ${COG0_SRC}/WorkingMemory.cpp
    ${COG0_SRC}/LongTermMemory.cpp
    ${COG0_SRC}/ConceptFormation.cpp
    ${COG0_SRC}/PatternDiscovery.cpp
    ${COG0_SRC}/RaftConsensus.cpp
    ${COG0_SRC}/ConflictResolver.cpp
    ${COG0_SRC}/MonitoringServer.cpp
)

# -----------------------------------------------------------------------
# cog0 shared library

ADD_LIBRARY(cog0 SHARED ${COG0_SOURCES})

TARGET_INCLUDE_DIRECTORIES(cog0
    PUBLIC
        $<BUILD_INTERFACE:${COG0_INC}>
        $<INSTALL_INTERFACE:include>
        ${COGUTIL_INCLUDE_DIRS}
        ${ATOMSPACE_INCLUDE_DIRS}
)

TARGET_LINK_LIBRARIES(cog0
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
    Threads::Threads
)

SET_TARGET_PROPERTIES(cog0 PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION 0
    OUTPUT_NAME "cog0"
)

# -----------------------------------------------------------------------
# Install

INSTALL(TARGETS cog0
    EXPORT  CogZeroTargets
    LIBRARY DESTINATION lib${LIB_DIR_SUFFIX}/opencog
    ARCHIVE DESTINATION lib${LIB_DIR_SUFFIX}/opencog
)

INSTALL(DIRECTORY ${COG0_INC}/
    DESTINATION include/opencog/cog0
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
)
