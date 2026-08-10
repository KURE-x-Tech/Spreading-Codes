cmake_minimum_required(VERSION 3.16)
project(LunaNetGateway VERSION 1.2.6 LANGUAGES CXX)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/lunanet/version.h
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(UNIX AND NOT APPLE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(LIBRARY_NAME lunanet_spreading_codes)

set(GATEWAY2_SOURCES
    codes/gateway2/bch_codec.cpp
    codes/gateway2/crc24.cpp
    codes/gateway2/interleaver.cpp
    codes/gateway2/ldpc_encoder.cpp
)

set(GATEWAY2_HEADERS
    codes/gateway2/bch_codec.h
    codes/gateway2/crc24.h
    codes/gateway2/interleaver.h
    codes/gateway2/ldpc_encoder.h
)

add_library(lunanet_gateway2 STATIC ${GATEWAY2_SOURCES} ${GATEWAY2_HEADERS})
target_include_directories(lunanet_gateway2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway2
)

set(GATEWAY3_SOURCES
    codes/gateway3/sync_pattern.cpp
    codes/gateway3/subframe1_builder.cpp
    codes/gateway3/subframe2_builder.cpp
    codes/gateway3/subframe3_builder.cpp
    codes/gateway3/subframe4_builder.cpp
    codes/gateway3/frame_assembler.cpp
    codes/gateway3/frame_exporter.cpp
)

set(GATEWAY3_HEADERS
    codes/gateway3/frame_config.h
    codes/gateway3/sync_pattern.h
    codes/gateway3/subframe1_builder.h
    codes/gateway3/subframe2_builder.h
    codes/gateway3/subframe3_builder.h
    codes/gateway3/subframe4_builder.h
    codes/gateway3/frame_assembler.h
    codes/gateway3/frame_exporter.h
)

add_library(lunanet_gateway3 STATIC ${GATEWAY3_SOURCES} ${GATEWAY3_HEADERS})
target_include_directories(lunanet_gateway3 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway2
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway3
)
target_link_libraries(lunanet_gateway3 PUBLIC lunanet_gateway2)

set(GATEWAY4_SOURCES
    codes/gateway4/bpsk_modulator.cpp
    codes/gateway4/iq_generator.cpp
    codes/gateway4/signal_exporter.cpp
)

set(GATEWAY4_HEADERS
    codes/gateway4/signal_config.h
    codes/gateway4/bpsk_modulator.h
    codes/gateway4/iq_generator.h
    codes/gateway4/signal_exporter.h
)

add_library(lunanet_gateway4 STATIC ${GATEWAY4_SOURCES} ${GATEWAY4_HEADERS})
target_include_directories(lunanet_gateway4 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway4
)

set(GATEWAY5_SOURCES
    codes/gateway5/frame_decoder.cpp
    codes/gateway5/frame_synchronizer.cpp
    codes/gateway5/sync_detector.cpp
    codes/gateway5/despreader.cpp
    codes/gateway1/spreading_config.cpp
    codes/gateway1/gold_code_generator.cpp
    codes/gateway5/symbol_extractor.cpp
    codes/gateway5/bch_soft_decoder.cpp
    codes/gateway5/deinterleaver.cpp
    codes/gateway5/ldpc_decoder.cpp
    codes/gateway5/crc_validator.cpp
)

set(GATEWAY5_HEADERS
    codes/gateway5/frame_decoder.h
    codes/gateway5/frame_synchronizer.h
    codes/gateway5/sync_detector.h
    codes/gateway5/despreader.h
    codes/gateway1/spreading_config.h
    codes/gateway1/gold_code_generator.h
    codes/gateway5/symbol_extractor.h
    codes/gateway5/bch_soft_decoder.h
    codes/gateway5/deinterleaver.h
    codes/gateway5/ldpc_decoder.h
    codes/gateway5/crc_validator.h
)

add_executable(gateway3_frame_exporter_test
    codes/gateway3/frame_exporter_test.cpp
)

target_link_libraries(gateway3_frame_exporter_test PRIVATE
    lunanet_gateway3
)

set_target_properties(gateway3_frame_exporter_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway3_frame_exporter_test
    COMMAND gateway3_frame_exporter_test
)

add_executable(gateway4_signal_exporter_test
    codes/gateway4/signal_exporter_test.cpp
)

target_link_libraries(gateway4_signal_exporter_test PRIVATE
    lunanet_gateway4
)

set_target_properties(gateway4_signal_exporter_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway4_signal_exporter_test
    COMMAND gateway4_signal_exporter_test
)

add_library(lunanet_gateway5 STATIC ${GATEWAY5_SOURCES} ${GATEWAY5_HEADERS})

target_include_directories(lunanet_gateway5 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway5
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway1
)

target_link_libraries(lunanet_gateway5 PUBLIC
    lunanet_gateway3
    lunanet_gateway4
)

add_executable(gateway5_frame_sync_test
    codes/gateway5/frame_sync_test.cpp
)

target_link_libraries(gateway5_frame_sync_test PRIVATE
    lunanet_gateway5
)

set_target_properties(gateway5_frame_sync_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_frame_sync_test
    COMMAND gateway5_frame_sync_test
)

add_executable(gateway5_sync_detector_test
    codes/gateway5/sync_detector_test.cpp
)

target_link_libraries(gateway5_sync_detector_test PRIVATE
    lunanet_gateway5
)

set_target_properties(gateway5_sync_detector_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_sync_detector_test
    COMMAND gateway5_sync_detector_test
)

add_executable(gateway5_despreader_test
    codes/gateway5/despreader_test.cpp
)

target_link_libraries(gateway5_despreader_test PRIVATE
    lunanet_gateway5
)

set_target_properties(gateway5_despreader_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_despreader_test
    COMMAND gateway5_despreader_test
)

add_executable(gateway5_symbol_extractor_test
    codes/gateway5/symbol_extractor_test.cpp
)

target_link_libraries(gateway5_symbol_extractor_test PRIVATE
    lunanet_gateway5
    lunanet_gateway2
)

target_include_directories(gateway5_symbol_extractor_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway2
)

set_target_properties(gateway5_symbol_extractor_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_symbol_extractor_test
    COMMAND gateway5_symbol_extractor_test
)

add_executable(gateway5_bch_decode_test
    codes/gateway5/bch_decode_test.cpp
)

target_link_libraries(gateway5_bch_decode_test PRIVATE
    lunanet_gateway5
    lunanet_gateway2
)

target_include_directories(gateway5_bch_decode_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway2
)

set_target_properties(gateway5_bch_decode_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_bch_decode_test
    COMMAND gateway5_bch_decode_test
)

add_executable(gateway5_deinterleave_test
    codes/gateway5/deinterleave_test.cpp
)

target_link_libraries(gateway5_deinterleave_test PRIVATE
    lunanet_gateway5
    lunanet_gateway2
)

target_include_directories(gateway5_deinterleave_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway2
)

set_target_properties(gateway5_deinterleave_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_deinterleave_test
    COMMAND gateway5_deinterleave_test
)

add_executable(gateway5_ldpc_decode_test
    codes/gateway5/ldpc_decode_test.cpp
)

target_link_libraries(gateway5_ldpc_decode_test PRIVATE
    lunanet_gateway5
    lunanet_gateway2
)

target_include_directories(gateway5_ldpc_decode_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway2
)

set_target_properties(gateway5_ldpc_decode_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_ldpc_decode_test
    COMMAND gateway5_ldpc_decode_test
)

add_executable(gateway5_crc_validation_test
    codes/gateway5/crc_validation_test.cpp
)

target_link_libraries(gateway5_crc_validation_test PRIVATE
    lunanet_gateway5
    lunanet_gateway2
)

target_include_directories(gateway5_crc_validation_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway2
)

set_target_properties(gateway5_crc_validation_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_crc_validation_test
    COMMAND gateway5_crc_validation_test
)

add_executable(gateway5_frame_decoder_test
    codes/gateway5/frame_decoder_test.cpp
)

target_link_libraries(gateway5_frame_decoder_test PRIVATE
    lunanet_gateway5
)

set_target_properties(gateway5_frame_decoder_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_frame_decoder_test
    COMMAND gateway5_frame_decoder_test
)

add_executable(gateway5_ber_benchmark
    codes/gateway5/ber_benchmark.cpp
)

target_link_libraries(gateway5_ber_benchmark PRIVATE
    lunanet_gateway5
)

set_target_properties(gateway5_ber_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_ber_qualification
    COMMAND gateway5_ber_benchmark
)

set_tests_properties(gateway5_ber_qualification PROPERTIES
    LABELS "gateway5;qualification;long"
    TIMEOUT 120
)

set(GATEWAY6_SOURCES
    codes/gateway6/subframe2_parser.cpp
    codes/gateway6/subframe3_parser.cpp
)

set(GATEWAY6_HEADERS
    codes/gateway6/subframe2_parser.h
    codes/gateway6/subframe3_parser.h
)

add_library(lunanet_gateway6 STATIC ${GATEWAY6_SOURCES} ${GATEWAY6_HEADERS})
target_include_directories(lunanet_gateway6 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
)

add_executable(gateway6_subframe2_parser_test
    codes/gateway6/subframe2_parser_test.cpp
)

target_link_libraries(gateway6_subframe2_parser_test PRIVATE
    lunanet_gateway6
    lunanet_gateway3
)

target_include_directories(gateway6_subframe2_parser_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway3
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway6
)

set_target_properties(gateway6_subframe2_parser_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway6_subframe2_parser_test
    COMMAND gateway6_subframe2_parser_test
)

add_executable(gateway6_subframe3_parser_test
    codes/gateway6/subframe3_parser_test.cpp
)

target_link_libraries(gateway6_subframe3_parser_test PRIVATE
    lunanet_gateway6
    lunanet_gateway3
)

target_include_directories(gateway6_subframe3_parser_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway3
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway6
)

set_target_properties(gateway6_subframe3_parser_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway6_subframe3_parser_test
    COMMAND gateway6_subframe3_parser_test
)

add_executable(gateway5_gateway6_handoff_test
    codes/gateway6/gateway5_handoff_test.cpp
)

target_link_libraries(gateway5_gateway6_handoff_test PRIVATE
    lunanet_gateway5
    lunanet_gateway6
)

set_target_properties(gateway5_gateway6_handoff_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gateway5_gateway6_handoff_test
    COMMAND gateway5_gateway6_handoff_test
)

set(GATEWAY1_SOURCES
    codes/spreading_codes.cpp
    codes/gateway1/spreading_config.cpp
    codes/gateway1/gold_code_generator.cpp
    codes/gateway1/weil_code_generator.cpp
    codes/gateway1/tiered_code_generator.cpp
    codes/c_api.cpp
)

set(GATEWAY1_HEADERS
    codes/spreading_codes.h
    codes/gateway1/spreading_config.h
    codes/gateway1/gold_code_generator.h
    codes/gateway1/weil_code_generator.h
    codes/gateway1/tiered_code_generator.h
    codes/c_api.h
)

add_library(${LIBRARY_NAME} SHARED ${GATEWAY1_SOURCES} ${GATEWAY1_HEADERS})
target_include_directories(${LIBRARY_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway1
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway2
)
target_include_directories(${LIBRARY_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_compile_definitions(${LIBRARY_NAME} PRIVATE LUNANET_BUILDING_DLL)
target_link_libraries(${LIBRARY_NAME} PRIVATE lunanet_gateway2)
set_target_properties(${LIBRARY_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

set(TESTING_SOURCES
    codes/testing/test_reporter.cpp
    codes/testing/test_annex3_loader.cpp
    codes/testing/test_validators.cpp
)

add_library(lunanet_testing STATIC ${TESTING_SOURCES})
target_include_directories(lunanet_testing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/testing
)
target_link_libraries(lunanet_testing PUBLIC ${LIBRARY_NAME})

add_executable(test_engine
    codes/test_engine.cpp
)
target_link_libraries(test_engine PRIVATE lunanet_testing lunanet_gateway2 lunanet_gateway3 lunanet_gateway4)
target_include_directories(test_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway3
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway4
)
set_target_properties(test_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_executable(goon codes/lsis_cli.cpp)
target_link_libraries(goon PRIVATE
    ${LIBRARY_NAME}
    lunanet_gateway5
)
target_include_directories(goon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/codes
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway3
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway4
    ${CMAKE_CURRENT_SOURCE_DIR}/codes/gateway5
)
set_target_properties(goon PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(APPLE)
    set_target_properties(goon PROPERTIES
        INSTALL_RPATH "@loader_path/../lib"
    )
elseif(UNIX)
    set_target_properties(goon PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
    )
endif()

enable_testing()
add_test(
    NAME goon_version_test
    COMMAND goon version
)
set_tests_properties(goon_version_test PROPERTIES
    PASS_REGULAR_EXPRESSION "^goon ${PROJECT_VERSION}"
)

add_test(
    NAME goon_cli_smoke_test
    COMMAND ${CMAKE_COMMAND}
        -DGOON_EXECUTABLE=$<TARGET_FILE:goon>
        -DEXPECTED_VERSION=${PROJECT_VERSION}
        -DWORK_DIR=${CMAKE_BINARY_DIR}/goon-cli-smoke
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/goon_cli_smoke.cmake
)
set_tests_properties(goon_cli_smoke_test PROPERTIES
    LABELS "cli;qa"
)

add_test(
    NAME goon_installed_package_test
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DINSTALL_CONFIG=$<CONFIG>
        -DGOON_FILENAME=$<TARGET_FILE_NAME:goon>
        -DEXPECTED_VERSION=${PROJECT_VERSION}
        -DSMOKE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/goon_cli_smoke.cmake
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/goon_installed_package_smoke.cmake
)
set_tests_properties(goon_installed_package_test PROPERTIES
    LABELS "cli;qa;install"
    RUN_SERIAL TRUE
    TIMEOUT 120
)

add_test(
    NAME gateway1_validation
    COMMAND test_engine
        ${CMAKE_CURRENT_SOURCE_DIR}/config/spreading_codes_config.ini
        --gateway gateway1
)

add_test(
    NAME gateway2_validation
    COMMAND test_engine
        ${CMAKE_CURRENT_SOURCE_DIR}/config/spreading_codes_config.ini
        --gateway gateway2
)

add_test(
    NAME gateway3_validation
    COMMAND test_engine
        ${CMAKE_CURRENT_SOURCE_DIR}/config/spreading_codes_config.ini
        --gateway gateway3
)

add_test(
    NAME gateway4_validation
    COMMAND test_engine
        ${CMAKE_CURRENT_SOURCE_DIR}/config/spreading_codes_config.ini
        --gateway gateway4
)

install(TARGETS ${LIBRARY_NAME} goon
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${GATEWAY1_HEADERS}
    DESTINATION include/lunanet
)

install(FILES config/spreading_codes_config.ini
    DESTINATION share/lunanet/config
)

install(DIRECTORY docs/spec_tables/
    DESTINATION share/lunanet/docs/spec_tables
    FILES_MATCHING
    PATTERN "*.csv"
    PATTERN "._*" EXCLUDE
)

install(DIRECTORY Validation/annex3/txt/
    DESTINATION share/lunanet/Validation/annex3/txt
    FILES_MATCHING
    PATTERN "*.txt"
    PATTERN "generated" EXCLUDE
    PATTERN "._*" EXCLUDE
)

install(DIRECTORY Validation/annex3/csv/
    DESTINATION share/lunanet/Validation/annex3/csv
    FILES_MATCHING
    PATTERN "*.csv"
    PATTERN "._*" EXCLUDE
)
