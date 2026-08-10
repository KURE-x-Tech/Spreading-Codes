if(NOT DEFINED BUILD_DIR OR
   NOT DEFINED GOON_FILENAME OR
   NOT DEFINED EXPECTED_VERSION OR
   NOT DEFINED SMOKE_SCRIPT)
    message(FATAL_ERROR
        "BUILD_DIR, GOON_FILENAME, EXPECTED_VERSION, and SMOKE_SCRIPT are required")
endif()

if(WIN32)
    set(temp_root "$ENV{TEMP}")
elseif(DEFINED ENV{TMPDIR} AND NOT "$ENV{TMPDIR}" STREQUAL "")
    set(temp_root "$ENV{TMPDIR}")
else()
    set(temp_root "/tmp")
endif()

if(temp_root STREQUAL "")
    message(FATAL_ERROR "No operating-system temporary directory is available")
endif()

set(INSTALL_PREFIX "${temp_root}/lunanet-goon-package-${EXPECTED_VERSION}")
set(WORK_DIR "${temp_root}/lunanet-goon-package-work-${EXPECTED_VERSION}")
file(REMOVE_RECURSE "${INSTALL_PREFIX}" "${WORK_DIR}")

set(install_command
    "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}")
if(DEFINED INSTALL_CONFIG AND NOT INSTALL_CONFIG STREQUAL "")
    list(APPEND install_command --config "${INSTALL_CONFIG}")
endif()

execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
        "staged goon install failed: result=${install_result}, output=${install_output}, error=${install_error}")
endif()

set(GOON_EXECUTABLE "${INSTALL_PREFIX}/bin/${GOON_FILENAME}")
if(NOT EXISTS "${GOON_EXECUTABLE}")
    message(FATAL_ERROR "staged goon executable was not installed: ${GOON_EXECUTABLE}")
endif()

include("${SMOKE_SCRIPT}")
file(REMOVE_RECURSE "${INSTALL_PREFIX}" "${WORK_DIR}")
message(STATUS "installed goon package smoke test passed")