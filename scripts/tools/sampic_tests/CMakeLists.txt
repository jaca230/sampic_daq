cmake_minimum_required(VERSION 3.18)
project(sampic_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(SAMPIC_VENDOR_ROOT ${PROJECT_ROOT}/external/sampic_256ch_lib)
set(SAMPIC_VENDOR_INCLUDE ${SAMPIC_VENDOR_ROOT}/include_lib)
set(SAMPIC_VENDOR_LIB_DIR ${SAMPIC_VENDOR_ROOT}/lib)
set(LPDEVC_INCLUDE_DIR ${SAMPIC_VENDOR_ROOT}/lpdevc_install/include)
set(LPDEVC_LIB_DIR ${SAMPIC_VENDOR_ROOT}/lpdevc_install/lib)

include(${PROJECT_ROOT}/cmake/CPM.cmake)

foreach(path ${SAMPIC_VENDOR_INCLUDE} ${LPDEVC_INCLUDE_DIR} ${LPDEVC_LIB_DIR} ${SAMPIC_VENDOR_LIB_DIR})
  if(NOT EXISTS ${path})
    message(FATAL_ERROR "Required vendor component missing: ${path}. Build the vendor library first.")
  endif()
endforeach()

find_library(FTD2XX_LIB_PATH
  NAMES ftd2xx libftd2xx.so libftd2xx.so.1.4.33 libftd2xx.so.1.4.8
  PATHS
    ${SAMPIC_VENDOR_ROOT}/lpdevc_install
    ${SAMPIC_VENDOR_ROOT}/lpdevc_install/lib
  NO_DEFAULT_PATH)
if(NOT FTD2XX_LIB_PATH)
  message(FATAL_ERROR "Unable to locate libftd2xx. Run the vendor installer.")
endif()
get_filename_component(FTD2XX_LIB_DIR ${FTD2XX_LIB_PATH} DIRECTORY)

find_package(Threads REQUIRED)

CPMAddPackage(
  NAME nlohmann_json
  GITHUB_REPOSITORY nlohmann/json
  VERSION 3.11.2
)

CPMAddPackage(
  NAME spdlog
  GITHUB_REPOSITORY gabime/spdlog
  VERSION 1.12.0
  OPTIONS "SPDLOG_INSTALL OFF"
)

add_executable(sampic_deadtime_scan
  src/app/main.cpp
  src/app/mode_registry.cpp
  src/lecroy/lecroy_client.cpp
  src/lecroy/manual_trigger_controller.cpp
  src/modes/crate_smoke/crate_smoke_mode.cpp
  src/modes/deadtime/config.cpp
  src/modes/deadtime/mode.cpp
  src/modes/deadtime/scan_runner.cpp
  src/modes/deadtime/sampic_session.cpp
  src/modes/double_pulse/config.cpp
  src/modes/double_pulse/mode.cpp
  src/modes/double_pulse/scan_runner.cpp
  src/modes/double_pulse/sampic_session.cpp
  src/modes/occupancy/occupancy_mode.cpp
  src/common/scan_types.cpp
  src/common/time_utils.cpp
  src/modes/pulser/pulser_mode.cpp
  ${PROJECT_ROOT}/src/integration/sampic/config/sampic_crate_configurator.cpp
  ${PROJECT_ROOT}/src/integration/sampic/config/sampic_board_configurator.cpp
  ${PROJECT_ROOT}/src/integration/sampic/config/sampic_chip_configurator.cpp
  ${PROJECT_ROOT}/src/integration/sampic/config/sampic_channel_configurator.cpp
  ${PROJECT_ROOT}/src/integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_default.cpp)

add_executable(lecroy_test
  src/tests/lecroy_test.cpp
  src/lecroy/lecroy_client.cpp)

add_executable(delay_separation_test
  src/tests/delay_separation_test.cpp
  src/modes/double_pulse/config.cpp
  src/modes/double_pulse/sampic_session.cpp
  src/lecroy/lecroy_client.cpp
  src/lecroy/manual_trigger_controller.cpp
  src/common/scan_types.cpp
  src/common/time_utils.cpp)

add_executable(external_trigger_probe
  src/tests/external_trigger_probe.cpp
  src/modes/double_pulse/config.cpp
  src/lecroy/lecroy_client.cpp
  src/lecroy/manual_trigger_controller.cpp)

add_executable(board_probe
  src/tests/board_probe.cpp)

add_executable(sampic_hit_dump
  src/tests/sampic_hit_dump.cpp)

target_include_directories(sampic_deadtime_scan PRIVATE
  ${SAMPIC_VENDOR_INCLUDE}
  ${LPDEVC_INCLUDE_DIR}
  ${nlohmann_json_SOURCE_DIR}/include
  ${spdlog_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${PROJECT_ROOT}/include)

target_include_directories(lecroy_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${nlohmann_json_SOURCE_DIR}/include)

target_include_directories(delay_separation_test PRIVATE
  ${SAMPIC_VENDOR_INCLUDE}
  ${LPDEVC_INCLUDE_DIR}
  ${nlohmann_json_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(external_trigger_probe PRIVATE
  ${SAMPIC_VENDOR_INCLUDE}
  ${LPDEVC_INCLUDE_DIR}
  ${nlohmann_json_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(sampic_hit_dump PRIVATE
  ${SAMPIC_VENDOR_INCLUDE}
  ${LPDEVC_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(board_probe PRIVATE
  ${SAMPIC_VENDOR_INCLUDE}
  ${LPDEVC_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SAMPIC_VENDOR_LIBS
  ${SAMPIC_VENDOR_LIB_DIR}/libsampic256ch.so
  ${LPDEVC_LIB_DIR}/liblpdevC.so
  ${LPDEVC_LIB_DIR}/liblpdev.so
  ${FTD2XX_LIB_PATH})

set(SAMPIC_TEST_TARGETS
  sampic_deadtime_scan
  delay_separation_test
  external_trigger_probe
  sampic_hit_dump
  board_probe)

foreach(target ${SAMPIC_TEST_TARGETS})
  target_link_libraries(${target} PRIVATE
    ${SAMPIC_VENDOR_LIBS}
    Threads::Threads
    dl rt pthread m)
endforeach()

foreach(target ${SAMPIC_TEST_TARGETS})
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    BUILD_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
    INSTALL_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}")
endforeach()

set_target_properties(lecroy_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_target_properties(external_trigger_probe PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_target_properties(sampic_hit_dump PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

install(TARGETS ${SAMPIC_TEST_TARGETS} lecroy_test external_trigger_probe sampic_hit_dump RUNTIME DESTINATION bin)
