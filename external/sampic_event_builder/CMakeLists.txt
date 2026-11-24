cmake_minimum_required(VERSION 3.20)
project(sampic_event_builder VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

# The internal raw stream uses the actual vendor HitStruct. Match the crate
# frontend's header selection; no vendor library linkage is needed here.
set(SAMPIC_LIBRARY_VERSION "3.2" CACHE STRING "SAMPIC vendor headers (3.2 or 3.1)")
set_property(CACHE SAMPIC_LIBRARY_VERSION PROPERTY STRINGS 3.2 3.1)
if(SAMPIC_LIBRARY_VERSION STREQUAL "3.2")
  set(_vendor_root "${CMAKE_CURRENT_SOURCE_DIR}/../sampic_256ch_lib_3_2")
elseif(SAMPIC_LIBRARY_VERSION STREQUAL "3.1")
  set(_vendor_root "${CMAKE_CURRENT_SOURCE_DIR}/../sampic_256ch_lib")
else()
  message(FATAL_ERROR "Unsupported SAMPIC_LIBRARY_VERSION; choose 3.2 or 3.1")
endif()
set(SAMPIC_SOURCE_ROOT_OVERRIDE "" CACHE PATH "Optional SAMPIC source package override")
if(SAMPIC_SOURCE_ROOT_OVERRIDE)
  set(_vendor_root "${SAMPIC_SOURCE_ROOT_OVERRIDE}")
endif()
if(NOT EXISTS "${_vendor_root}/include_lib/SAMPIC_256Ch_Type.h")
  message(FATAL_ERROR "Missing SAMPIC vendor headers at ${_vendor_root}/include_lib")
endif()

add_library(sampic_event_builder_core
  src/diagnostics/timing_accumulator.cpp
  src/metrics/stage_metrics.cpp
  src/pipeline/event_builder_pipeline.cpp
  src/banks/data_bank_decoder.cpp
  src/banks/data_bank_encoder.cpp
  src/stages/building/fixed_time_bin_event_builder.cpp
  src/stages/building/periodic_readout_event_builder.cpp
  src/stages/building/time_window_event_builder.cpp
  src/stages/finalization/product_bank_finalization_stage.cpp
  src/stages/input/midas_raw_input_stage.cpp
  src/stages/selection/topology_selection_stage.cpp)
target_include_directories(sampic_event_builder_core PUBLIC include "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
  "${_vendor_root}/include_lib")
target_compile_options(sampic_event_builder_core PRIVATE -Wall -Wextra -Wpedantic)

option(SAMPIC_EVENT_BUILDER_BUILD_MIDAS_FRONTEND
  "Build the MIDAS event-builder frontend" OFF)
if(SAMPIC_EVENT_BUILDER_BUILD_MIDAS_FRONTEND)
  if(NOT DEFINED ENV{MIDASSYS})
    message(FATAL_ERROR "MIDASSYS is required for the MIDAS frontend")
  endif()
  find_package(Threads REQUIRED)
  set(MIDASSYS_ROOT "$ENV{MIDASSYS}")
  add_executable(sampic_event_builder_frontend frontend.cpp)
  target_include_directories(sampic_event_builder_frontend PRIVATE
    "${MIDASSYS_ROOT}/include")
  target_link_libraries(sampic_event_builder_frontend PRIVATE
    sampic_event_builder_core
    "${MIDASSYS_ROOT}/lib/libmfe.a"
    "${MIDASSYS_ROOT}/lib/libmidas.a"
    Threads::Threads rt dl util)
endif()

option(SAMPIC_EVENT_BUILDER_BUILD_BENCHMARKS "Build rate-test executables" OFF)
if(SAMPIC_EVENT_BUILDER_BUILD_BENCHMARKS)
  add_executable(sampic_event_builder_benchmark
    tests/benchmarks/event_builder_benchmark.cpp
    tests/support/src/crate_simulator.cpp)
  target_include_directories(sampic_event_builder_benchmark PRIVATE tests/support/include)
  target_link_libraries(sampic_event_builder_benchmark PRIVATE sampic_event_builder_core)
endif()

if(BUILD_TESTING)
  add_executable(sampic_event_builder_tests
    tests/event_builder_tests.cpp
    tests/support/src/crate_simulator.cpp)
  target_include_directories(sampic_event_builder_tests PRIVATE tests/support/include)
  target_link_libraries(sampic_event_builder_tests PRIVATE sampic_event_builder_core)
  add_test(NAME sampic_event_builder_tests COMMAND sampic_event_builder_tests)
endif()

if(BUILD_TESTING)
  target_compile_options(sampic_event_builder_tests PRIVATE -UNDEBUG)
endif()

if(BUILD_TESTING AND SAMPIC_EVENT_BUILDER_BUILD_MIDAS_FRONTEND)
  add_executable(midas_bank_writer_tests tests/midas_bank_writer_tests.cpp)
  target_include_directories(midas_bank_writer_tests PRIVATE "${MIDASSYS_ROOT}/include")
  target_link_libraries(midas_bank_writer_tests PRIVATE sampic_event_builder_core
    "${MIDASSYS_ROOT}/lib/libmidas.a" Threads::Threads rt dl util)
  target_compile_options(midas_bank_writer_tests PRIVATE -UNDEBUG)
  add_test(NAME midas_bank_writer_tests COMMAND midas_bank_writer_tests)
endif()

if(BUILD_TESTING)
  add_executable(selection_tests tests/selection_tests.cpp)
  target_include_directories(selection_tests PRIVATE tests/support/include)
  target_link_libraries(selection_tests PRIVATE sampic_event_builder_core)
  target_compile_options(selection_tests PRIVATE -UNDEBUG)
  add_test(NAME selection_tests COMMAND selection_tests)
endif()
