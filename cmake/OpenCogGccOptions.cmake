# ===============================================================
# Detect different compilers and OS'es, tweak flags as necessary.
#
# The C++ standard is controlled by CMAKE_CXX_STANDARD in the project's
# root CMakeLists.txt; do NOT hardcode -std=... flags here.

IF (CMAKE_COMPILER_IS_GNUCXX)
	# GCC 9 or later is required for reliable C++17 support.
	IF (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
		MESSAGE(FATAL_ERROR "GCC version must be at least 9.0!")
	ENDIF (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)

	IF (APPLE)
		CMAKE_POLICY(SET CMP0042 NEW)

		SET(CMAKE_C_FLAGS "-Wall -Wno-long-long -Wno-conversion")
		SET(CMAKE_C_FLAGS_DEBUG "-O0 -g")
		SET(CMAKE_C_FLAGS_PROFILE "-O0 -pg")
		SET(CMAKE_C_FLAGS_RELEASE "-O2 -g0")
		SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,-flat_namespace,-undefined,dynamic_lookup")
		SET(CMAKE_EXE_LINKER_FLAGS "-Wl,-flat_namespace,-undefined,dynamic_lookup")
		SET(NO_AS_NEEDED "")

	ELSE (APPLE)
		SET(CMAKE_C_FLAGS "-Wall -fPIC -fstack-protector")
		SET(CMAKE_C_FLAGS_ASAN "-O3 -g -fsanitize=address,undefined -Wformat -Werror=format-security -Werror=array-bounds")
		SET(CMAKE_C_FLAGS_DEBUG "-O0 -ggdb3")
		SET(CMAKE_C_FLAGS_PROFILE "-O2 -g3 -pg")
		SET(CMAKE_C_FLAGS_RELEASE "-O3 -g -flto=auto")
		SET(NO_AS_NEEDED "-Wl,--no-as-needed")
		LINK_LIBRARIES(pthread)
		SET(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} <LINK_LIBRARIES>")
	ENDIF (APPLE)

	# Inherit C flags and add C++-specific suppressions; the standard is
	# set via CMAKE_CXX_STANDARD rather than a hardcoded -std flag.
	SET(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-volatile -Wno-variadic-macros -fopenmp")
	SET(CMAKE_CXX_FLAGS_ASAN ${CMAKE_C_FLAGS_ASAN})
	SET(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
	SET(CMAKE_CXX_FLAGS_PROFILE ${CMAKE_C_FLAGS_PROFILE})
	SET(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})

	SET(CMAKE_C_FLAGS_COVERAGE "-O0 -g -fprofile-arcs -ftest-coverage -fno-inline")
	SET(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_C_FLAGS_COVERAGE} -fno-default-inline")
	IF (CMAKE_BUILD_TYPE STREQUAL "Coverage")
		LINK_LIBRARIES(gcov)
	ENDIF (CMAKE_BUILD_TYPE STREQUAL "Coverage")
ENDIF (CMAKE_COMPILER_IS_GNUCXX)

IF (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	SET(CMAKE_SHARED_LINKER_FLAGS "-undefined dynamic_lookup")
	SET(CMAKE_EXE_LINKER_FLAGS "-lstdc++")
	# The C++ standard is controlled by CMAKE_CXX_STANDARD; no -std flag here.
ENDIF (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
