if(NOT DEFINED GOON_EXECUTABLE OR NOT DEFINED EXPECTED_VERSION OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "GOON_EXECUTABLE, EXPECTED_VERSION, and WORK_DIR are required")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/config")
file(WRITE "${WORK_DIR}/config/spreading_codes_config.ini"
    "invalid_working_directory_config=true\n")

execute_process(
    COMMAND "${GOON_EXECUTABLE}" version
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE version_result
    OUTPUT_VARIABLE version_output
    ERROR_VARIABLE version_error
)
string(STRIP "${version_output}" version_output)
if(NOT version_result EQUAL 0 OR
   NOT version_output STREQUAL "goon ${EXPECTED_VERSION}")
    message(FATAL_ERROR
        "goon version failed: result=${version_result}, output=${version_output}, error=${version_error}")
endif()

set(gold_output "${WORK_DIR}/gold.txt")
execute_process(
    COMMAND "${GOON_EXECUTABLE}"
        generate-codes
        --codes gold
        --output "${gold_output}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE gold_result
    OUTPUT_VARIABLE gold_stdout
    ERROR_VARIABLE gold_stderr
)
if(NOT gold_result EQUAL 0)
    message(FATAL_ERROR
        "goon generate-codes failed: result=${gold_result}, stdout=${gold_stdout}, stderr=${gold_stderr}")
endif()
file(STRINGS "${gold_output}" gold_lines)
list(LENGTH gold_lines gold_line_count)
if(NOT gold_line_count EQUAL 210)
    message(FATAL_ERROR "goon generated ${gold_line_count} Gold-code lines; expected 210")
endif()

set(frame_output "${WORK_DIR}/frame.bin")
execute_process(
    COMMAND "${GOON_EXECUTABLE}"
        encode
        --format frame
        --prn 7
        --fid 2
        --toi 73
        --wn 1234
        --itow 256
        --output "${frame_output}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE frame_result
    OUTPUT_VARIABLE frame_stdout
    ERROR_VARIABLE frame_stderr
)
if(NOT frame_result EQUAL 0)
    message(FATAL_ERROR
        "goon encode failed: result=${frame_result}, stdout=${frame_stdout}, stderr=${frame_stderr}")
endif()
file(SIZE "${frame_output}" frame_size)
if(NOT frame_size EQUAL 6000)
    message(FATAL_ERROR "goon generated a ${frame_size}-byte frame; expected 6000")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
message(STATUS "goon CLI smoke test passed for version ${EXPECTED_VERSION}")
