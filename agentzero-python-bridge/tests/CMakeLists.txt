# agentzero-python-bridge/tests/CMakeLists.txt

# Python ctypes / package tests — require the shared C API library
if(TARGET cog0_capi AND Python3_EXECUTABLE)
    add_test(
        NAME agentzero_python_bridge_tests
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/test_cog0_bridge.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    set_tests_properties(agentzero_python_bridge_tests PROPERTIES
        LABELS "python;phase9;agentzero-python-bridge"
        ENVIRONMENT "COG0_CAPI_LIB=$<TARGET_FILE:cog0_capi>;PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/.."
        TIMEOUT 60
    )
endif()
