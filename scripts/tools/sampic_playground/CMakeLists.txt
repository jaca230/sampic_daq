cmake_minimum_required(VERSION 3.20)
project(sampic_playground VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

include("${ROOT_DIR}/cmake/CPM.cmake")
include("${ROOT_DIR}/cmake/CPMConfig.cmake")

set(CPM_HEADER_INCLUDE_DIRS "")
set(CPM_LINK_TARGETS "")
foreach(pkg IN LISTS CPM_PACKAGE_LIST)
  set(pkg_repo "${${pkg}_REPO}")
  set(pkg_tag "${${pkg}_TAG}")
  set(pkg_target "${${pkg}_TARGET}")
  set(pkg_download_only "${${pkg}_DOWNLOAD_ONLY}")

  if(NOT pkg_repo)
    message(FATAL_ERROR "Missing CPM repository for package ${pkg}")
  endif()

  set(CPM_ARGS NAME ${pkg} GITHUB_REPOSITORY "${pkg_repo}")
  if(pkg_tag)
    list(APPEND CPM_ARGS GIT_TAG "${pkg_tag}")
    string(REGEX REPLACE "^v" "" pkg_version "${pkg_tag}")
    if(NOT pkg_version STREQUAL pkg_tag AND pkg_version MATCHES "^[0-9].*")
      list(APPEND CPM_ARGS VERSION "${pkg_version}")
    endif()
  endif()
  if(pkg_download_only)
    list(APPEND CPM_ARGS DOWNLOAD_ONLY YES)
  endif()

  CPMAddPackage(${CPM_ARGS})

  if(pkg_download_only)
    set(pkg_include_dir "${${pkg}_SOURCE_DIR}/include")
    if(NOT EXISTS "${pkg_include_dir}")
      set(pkg_include_dir "${${pkg}_SOURCE_DIR}")
    endif()
    list(APPEND CPM_HEADER_INCLUDE_DIRS "${pkg_include_dir}")
  elseif(pkg_target AND TARGET "${pkg_target}")
    list(APPEND CPM_LINK_TARGETS "${pkg_target}")
  elseif(TARGET "${pkg}")
    list(APPEND CPM_LINK_TARGETS "${pkg}")
  endif()
endforeach()

list(REMOVE_DUPLICATES CPM_HEADER_INCLUDE_DIRS)
list(REMOVE_DUPLICATES CPM_LINK_TARGETS)

find_package(Threads REQUIRED)

# Collect all necessary sources from parent project
set(PLAYGROUND_SOURCES
  "${ROOT_DIR}/src/core/threading/polling_worker.cpp"

  # SAMPIC collector sources
  "${ROOT_DIR}/src/integration/sampic/collector/sampic_event.cpp"
  "${ROOT_DIR}/src/integration/sampic/collector/sampic_event_buffer.cpp"
  "${ROOT_DIR}/src/integration/sampic/collector/sampic_collector.cpp"
  "${ROOT_DIR}/src/integration/sampic/collector/modes/simulator/simulator_mode.cpp"
  "${ROOT_DIR}/src/integration/sampic/collector/modes/default/default_mode.cpp"
  "${ROOT_DIR}/src/integration/sampic/collector/modes/example/example_mode.cpp"

  # Frontend collector sources
  "${ROOT_DIR}/src/processing/sampic_processing/collector/frontend_event.cpp"
  "${ROOT_DIR}/src/processing/sampic_processing/collector/frontend_event_buffer.cpp"
  "${ROOT_DIR}/src/processing/sampic_processing/collector/frontend_event_collector.cpp"
  "${ROOT_DIR}/src/processing/sampic_processing/collector/frontend_diagnostics.cpp"
  "${ROOT_DIR}/src/processing/sampic_processing/collector/modes/default/default_mode.cpp"
  "${ROOT_DIR}/src/processing/sampic_processing/collector/modes/external_trigger/external_trigger_mode.cpp"
  "${ROOT_DIR}/src/processing/sampic_processing/collector/banks/frontend_event_bank_data.cpp"
  "${ROOT_DIR}/src/processing/sampic_processing/collector/banks/frontend_event_bank_event_timing.cpp"
  "${ROOT_DIR}/src/processing/sampic_processing/collector/banks/frontend_event_bank_collector_timing.cpp"
  "${ROOT_DIR}/src/processing/sampic_processing/collector/banks/frontend_event_bank_trigger_metadata.cpp"
)

add_executable(sampic_playground
  src/main.cpp
  src/playground_runtime.cpp
  src/fake_midas_logger.cpp
  ${PLAYGROUND_SOURCES}
)

target_include_directories(sampic_playground PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${ROOT_DIR}/include"
  "${ROOT_DIR}/src"
  "${ROOT_DIR}/external/sampic_256ch_lib/include_lib"
  "${ROOT_DIR}/external/sampic_256ch_lib/lpdevc_install/include"
  ${CPM_HEADER_INCLUDE_DIRS}
)

if(CPM_LINK_TARGETS)
  target_link_libraries(sampic_playground PRIVATE ${CPM_LINK_TARGETS})
endif()

# Link against SAMPIC vendor library
set(SAMPIC_LIB_DIR "${ROOT_DIR}/external/sampic_256ch_lib/lib")
set(LPDEVC_LIB_DIR "${ROOT_DIR}/external/sampic_256ch_lib/lpdevc_install/lib")

target_link_libraries(sampic_playground PRIVATE
  Threads::Threads
  "${SAMPIC_LIB_DIR}/libsampic256ch.so"
  "${LPDEVC_LIB_DIR}/liblpdevC.so"
  "${LPDEVC_LIB_DIR}/liblpdev.so"
)

# Aggressive optimizations for performance benchmarking
target_compile_options(sampic_playground PRIVATE
  -O3
  -march=native
  -mtune=native
  -ffast-math
  -funroll-loops
)

set_target_properties(sampic_playground PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  BUILD_RPATH "${SAMPIC_LIB_DIR};${LPDEVC_LIB_DIR}"
  INSTALL_RPATH "${SAMPIC_LIB_DIR};${LPDEVC_LIB_DIR}"
)

message(STATUS "========================================")
message(STATUS "SAMPIC Playground Configuration:")
message(STATUS "  Output: ${CMAKE_BINARY_DIR}/bin/sampic_playground")
message(STATUS "  Optimizations: -O3 -march=native")
message(STATUS "========================================")
