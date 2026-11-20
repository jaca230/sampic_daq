cmake_minimum_required(VERSION 3.20)
project(sampic_event_builder VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

option(SAMPIC_EVENT_BUILDER_USE_IN_TREE_FITTER "Build the sibling template-fitter project" ON)
option(SAMPIC_EVENT_BUILDER_USE_IN_TREE_COMPRESSION "Build the sibling residual-compression project" ON)

if(SAMPIC_EVENT_BUILDER_USE_IN_TREE_FITTER)
  add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../template_fitter"
    "${CMAKE_CURRENT_BINARY_DIR}/template_fitter")
else()
  find_package(TemplateFitter CONFIG REQUIRED)
endif()
if(SAMPIC_EVENT_BUILDER_USE_IN_TREE_COMPRESSION)
  add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../residual_compression"
    "${CMAKE_CURRENT_BINARY_DIR}/residual_compression")
else()
  find_package(ResidualCompression CONFIG REQUIRED)
endif()

add_library(sampic_event_builder_core
  src/metrics/stage_metrics.cpp
  src/pipeline/event_builder_pipeline.cpp
  src/serialization/product_bank_decoder.cpp
  src/serialization/product_bank_encoder.cpp
  src/stages/building/time_window_event_builder.cpp
  src/stages/compression/residual_compression_stage.cpp
  src/stages/compression/uncompressed_stage.cpp
  src/stages/finalization/product_bank_finalization_stage.cpp
  src/stages/fitting/passthrough_fitting_stage.cpp
  src/stages/fitting/template_fitting_stage.cpp
  src/stages/input/midas_raw_input_stage.cpp
  src/stages/selection/topology_selection_stage.cpp)
target_include_directories(sampic_event_builder_core PUBLIC include)
target_link_libraries(sampic_event_builder_core PUBLIC
  TemplateFitter::template_fitter ResidualCompression::residual_compression)
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
    benchmarks/event_builder_benchmark.cpp
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
