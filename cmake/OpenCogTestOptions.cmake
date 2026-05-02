# -------------------------------------------------
# OpenCog Testing Configuration
#
# Finds CxxTest, sets up the testing infrastructure, and provides
# helper macros for creating test targets.

FIND_PACKAGE(Cxxtest)
IF (CXXTEST_FOUND)
	MESSAGE(STATUS "CxxTest found.")
ELSE (CXXTEST_FOUND)
	MESSAGE(STATUS "CxxTest missing: needed for unit tests.")
ENDIF (CXXTEST_FOUND)

MACRO(OPENCOG_SETUP_TESTING)
	IF (CXXTEST_FOUND)
		ENABLE_TESTING()
		ADD_CUSTOM_TARGET(tests)
		ADD_SUBDIRECTORY(tests EXCLUDE_FROM_ALL)

		IF (CMAKE_BUILD_TYPE STREQUAL "Coverage")
			ADD_CUSTOM_TARGET(check
				WORKING_DIRECTORY tests
				COMMAND ${CMAKE_CTEST_COMMAND} --force-new-ctest-process --output-on-failure $(ARGS)
				COMMENT "Running tests with coverage..."
			)
		ELSE (CMAKE_BUILD_TYPE STREQUAL "Coverage")
			ADD_CUSTOM_TARGET(check
				DEPENDS tests
				WORKING_DIRECTORY tests
				COMMAND ${CMAKE_CTEST_COMMAND} --force-new-ctest-process --output-on-failure $(ARGS)
				COMMENT "Running tests..."
			)
		ENDIF (CMAKE_BUILD_TYPE STREQUAL "Coverage")
	ENDIF (CXXTEST_FOUND)
ENDMACRO(OPENCOG_SETUP_TESTING)

MACRO(OPENCOG_ADD_TEST_TARGET _target_name _working_dir _comment)
	IF (CXXTEST_FOUND)
		ADD_CUSTOM_TARGET(${_target_name}
			DEPENDS tests
			WORKING_DIRECTORY ${_working_dir}
			COMMAND ${CMAKE_CTEST_COMMAND} $(ARGS)
			COMMENT ${_comment}
		)
	ENDIF (CXXTEST_FOUND)
ENDMACRO(OPENCOG_ADD_TEST_TARGET)
