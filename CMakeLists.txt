cmake_minimum_required(VERSION 3.23)
project(sampic_frontend VERSION 1.0 LANGUAGES CXX C)

# --------------------------------------------------------------------------
# Global compiler settings
# --------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------------------------------
# Environment: MIDAS
# --------------------------------------------------------------------------
if(NOT DEFINED ENV{MIDASSYS})
  message(FATAL_ERROR "MIDASSYS environment variable not set. Source MIDAS environment first.")
endif()

set(MIDASSYS_ROOT "$ENV{MIDASSYS}")
set(MIDASSYS_INCLUDE_DIRS
  ${MIDASSYS_ROOT}/include
  ${MIDASSYS_ROOT}/mxml
  ${MIDASSYS_ROOT}/midasio
  ${MIDASSYS_ROOT}/mjson
)
foreach(MIDAS_INC IN LISTS MIDASSYS_INCLUDE_DIRS)
  if(NOT IS_DIRECTORY "${MIDAS_INC}")
    message(FATAL_ERROR "Expected MIDAS include directory not found: ${MIDAS_INC}")
  endif()
endforeach()
set(MIDASSYS_LIB_DIR "${MIDASSYS_ROOT}/lib")
foreach(MIDAS_LIB IN ITEMS libmidas.a libmfe.a)
  if(NOT EXISTS "${MIDASSYS_LIB_DIR}/${MIDAS_LIB}")
    message(FATAL_ERROR "Expected ${MIDAS_LIB} under ${MIDASSYS_LIB_DIR}")
  endif()
endforeach()

# --------------------------------------------------------------------------
# Dependencies via CPM (keep include path for downstream CPM usage)
# --------------------------------------------------------------------------
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPMConfig.cmake")
find_package(Threads REQUIRED)

# --------------------------------------------------------------------------
# Fetch CPM packages (header-only + transitive targets)
# --------------------------------------------------------------------------
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

# --------------------------------------------------------------------------
# Vendor library: sampic_256ch_lib
# --------------------------------------------------------------------------
set(SAMPIC_LIBRARY_VERSION "3.2" CACHE STRING "SAMPIC vendor library version (3.2 or 3.1)")
set_property(CACHE SAMPIC_LIBRARY_VERSION PROPERTY STRINGS 3.2 3.1)
set(SAMPIC_SOURCE_ROOT_OVERRIDE "" CACHE PATH "Optional SAMPIC source package override")
set(SAMPIC_TRANSPORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/sampic_256ch_lib" CACHE PATH
    "Root containing the lpdev/FTDI transport dependencies")

if(SAMPIC_SOURCE_ROOT_OVERRIDE)
  set(SAMPIC_SOURCE_ROOT "${SAMPIC_SOURCE_ROOT_OVERRIDE}")
elseif(SAMPIC_LIBRARY_VERSION STREQUAL "3.2")
  set(SAMPIC_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/sampic_256ch_lib_3_2")
elseif(SAMPIC_LIBRARY_VERSION STREQUAL "3.1")
  set(SAMPIC_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/sampic_256ch_lib")
else()
  message(FATAL_ERROR "Unsupported SAMPIC_LIBRARY_VERSION='${SAMPIC_LIBRARY_VERSION}'; choose 3.2 or 3.1")
endif()

if(NOT IS_DIRECTORY "${SAMPIC_SOURCE_ROOT}")
  message(FATAL_ERROR "Missing SAMPIC source package: ${SAMPIC_SOURCE_ROOT}")
endif()
if(NOT IS_DIRECTORY "${SAMPIC_TRANSPORT_ROOT}")
  message(FATAL_ERROR "Missing SAMPIC transport package: ${SAMPIC_TRANSPORT_ROOT}. Run `git submodule update --init --recursive`.")
endif()

set(SAMPIC_VENDOR_INCLUDE_DIR "${SAMPIC_SOURCE_ROOT}/include_lib")
set(LPDEVC_INCLUDE_DIR "${SAMPIC_TRANSPORT_ROOT}/lpdevc_install/include")
if(NOT IS_DIRECTORY "${LPDEVC_INCLUDE_DIR}")
  set(LPDEVC_INCLUDE_DIR "${SAMPIC_TRANSPORT_ROOT}/include")
endif()
foreach(VENDOR_INC IN ITEMS "${SAMPIC_VENDOR_INCLUDE_DIR}" "${LPDEVC_INCLUDE_DIR}")
  if(NOT IS_DIRECTORY "${VENDOR_INC}")
    message(FATAL_ERROR "Missing vendor include directory: ${VENDOR_INC}")
  endif()
endforeach()

# Locate FTDI headers (allow user installs to override)
find_path(FTD2XX_INCLUDE_DIR
  NAMES ftd2xx.h
  PATHS "${LPDEVC_INCLUDE_DIR}"
  NO_DEFAULT_PATH
)
if(NOT FTD2XX_INCLUDE_DIR)
  find_path(FTD2XX_INCLUDE_DIR NAMES ftd2xx.h)
endif()
if(NOT FTD2XX_INCLUDE_DIR)
  message(FATAL_ERROR "Unable to locate ftd2xx.h. Run the vendor FTDI install script.")
endif()

set(SAMPIC_VENDOR_LIB_DIR "${CMAKE_BINARY_DIR}/lib")
set(LPDEVC_LIB_DIR "${SAMPIC_TRANSPORT_ROOT}/lpdevc_install/lib")
if(NOT IS_DIRECTORY "${LPDEVC_LIB_DIR}")
  set(LPDEVC_LIB_DIR "${SAMPIC_TRANSPORT_ROOT}/lib")
endif()
foreach(REQUIRED_LIB_NAME IN ITEMS liblpdevC.so liblpdev.so)
  if(NOT EXISTS "${LPDEVC_LIB_DIR}/${REQUIRED_LIB_NAME}")
    message(FATAL_ERROR "Missing ${REQUIRED_LIB_NAME} in ${LPDEVC_LIB_DIR}. Did you run the vendor installer?")
  endif()
endforeach()

find_library(FTD2XX_LIB_PATH
  NAMES ftd2xx libftd2xx.so libftd2xx.so.1.4.33 libftd2xx.so.1.4.8
  PATHS
    "${SAMPIC_TRANSPORT_ROOT}/lpdevc_install"
    "${LPDEVC_LIB_DIR}"
  NO_DEFAULT_PATH
)
if(NOT FTD2XX_LIB_PATH)
  find_library(FTD2XX_LIB_PATH NAMES ftd2xx)
endif()
if(NOT FTD2XX_LIB_PATH)
  message(FATAL_ERROR "Unable to locate libftd2xx.so. Ensure FTDI runtime is installed.")
endif()
get_filename_component(FTD2XX_LIB_DIR "${FTD2XX_LIB_PATH}" DIRECTORY)

# Keep the unversioned FTDI runtime name in the build tree. Never generate
# files inside the legacy transport submodule.
get_filename_component(FTD2XX_BASENAME "${FTD2XX_LIB_PATH}" NAME)
if(NOT FTD2XX_BASENAME STREQUAL "libftd2xx.so")
  file(MAKE_DIRECTORY "${SAMPIC_VENDOR_LIB_DIR}")
  set(FTD2XX_SYMLINK "${SAMPIC_VENDOR_LIB_DIR}/libftd2xx.so")
  if(NOT EXISTS "${FTD2XX_SYMLINK}")
    message(STATUS "Creating symlink: libftd2xx.so -> ${FTD2XX_BASENAME}")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E create_symlink
              "${FTD2XX_LIB_PATH}" "${FTD2XX_SYMLINK}"
      RESULT_VARIABLE SYMLINK_RESULT
    )
    if(NOT SYMLINK_RESULT EQUAL 0)
      message(WARNING "Failed to create libftd2xx.so symlink. Build may fail.")
    endif()
  endif()
  set(FTD2XX_LIB_PATH "${FTD2XX_SYMLINK}")
  set(FTD2XX_LIB_DIR "${SAMPIC_VENDOR_LIB_DIR}")
endif()

# Imported vendor-shipped shared libraries
add_library(lpdevC SHARED IMPORTED GLOBAL)
set_target_properties(lpdevC PROPERTIES
  IMPORTED_LOCATION "${LPDEVC_LIB_DIR}/liblpdevC.so"
  INTERFACE_INCLUDE_DIRECTORIES "${LPDEVC_INCLUDE_DIR};${FTD2XX_INCLUDE_DIR}"
)

add_library(lpdev SHARED IMPORTED GLOBAL)
set_target_properties(lpdev PROPERTIES
  IMPORTED_LOCATION "${LPDEVC_LIB_DIR}/liblpdev.so"
  INTERFACE_INCLUDE_DIRECTORIES "${LPDEVC_INCLUDE_DIR};${FTD2XX_INCLUDE_DIR}"
)

add_library(ftd2xx SHARED IMPORTED GLOBAL)
set_target_properties(ftd2xx PROPERTIES
  IMPORTED_LOCATION "${FTD2XX_LIB_PATH}"
  INTERFACE_INCLUDE_DIRECTORIES "${FTD2XX_INCLUDE_DIR}"
)

# --------------------------------------------------------------------------
# Build the vendor Sampic library with CMake instead of Makefiles
# --------------------------------------------------------------------------
set(SAMPIC_VENDOR_SOURCES
  "${SAMPIC_SOURCE_ROOT}/sources_lib/SAMPIC_256Ch_hardware_core.c"
  "${SAMPIC_SOURCE_ROOT}/sources_lib/SAMPIC_256Ch_lib.c"
)
foreach(VENDOR_SRC IN LISTS SAMPIC_VENDOR_SOURCES)
  if(NOT EXISTS "${VENDOR_SRC}")
    message(FATAL_ERROR "Vendor source missing: ${VENDOR_SRC}")
  endif()
endforeach()

file(MAKE_DIRECTORY "${SAMPIC_VENDOR_LIB_DIR}")

add_library(sampic256ch SHARED ${SAMPIC_VENDOR_SOURCES})
target_include_directories(sampic256ch
  PUBLIC
    "${SAMPIC_VENDOR_INCLUDE_DIR}"
    "${LPDEVC_INCLUDE_DIR}"
    "${FTD2XX_INCLUDE_DIR}"
)
target_compile_definitions(sampic256ch PRIVATE __LINUX_BUILD=1)
target_link_libraries(sampic256ch
  PUBLIC
    lpdevC
    lpdev
    ftd2xx
    m
)
target_link_options(sampic256ch PRIVATE "LINKER:--disable-new-dtags")
set_target_properties(sampic256ch PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${SAMPIC_VENDOR_LIB_DIR}"
  OUTPUT_NAME "sampic256ch"
  BUILD_RPATH "${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
  INSTALL_RPATH "${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
)

message(STATUS "SAMPIC library version: ${SAMPIC_LIBRARY_VERSION}")
message(STATUS "SAMPIC source package: ${SAMPIC_SOURCE_ROOT}")
message(STATUS "SAMPIC transport package: ${SAMPIC_TRANSPORT_ROOT}")

# --------------------------------------------------------------------------
# Main frontend build
# --------------------------------------------------------------------------
set(PROJECT_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(MAIN_SOURCE "${PROJECT_SOURCE_ROOT}/frontend.cpp")
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_ROOT}/src/*.cpp")

add_executable(sampic_frontend
  "${MAIN_SOURCE}"
  ${PROJECT_SOURCES}
)

target_include_directories(sampic_frontend PRIVATE
  "${PROJECT_SOURCE_ROOT}/include"
  "${PROJECT_SOURCE_ROOT}/src"
  ${MIDASSYS_INCLUDE_DIRS}
  ${CPM_HEADER_INCLUDE_DIRS}
)
target_compile_definitions(
  sampic_frontend PRIVATE
  SAMPIC_DAQ_SOURCE_DIR="${PROJECT_SOURCE_ROOT}"
)

target_link_libraries(sampic_frontend PRIVATE
  sampic256ch
  ${CPM_LINK_TARGETS}
  Threads::Threads
  rt pthread dl util
  "${MIDASSYS_LIB_DIR}/libmfe.a"
  "${MIDASSYS_LIB_DIR}/libmidas.a"
)
target_link_options(sampic_frontend PRIVATE "LINKER:--disable-new-dtags")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(sampic_frontend PRIVATE -Wno-stringop-overflow -Wno-cpp)
endif()

# Aggressive optimizations for hot-path files
set_source_files_properties(
  "${PROJECT_SOURCE_ROOT}/src/integration/sampic/collector/modes/simulator/simulator_mode.cpp"
  "${PROJECT_SOURCE_ROOT}/src/integration/sampic/collector/sampic_event.cpp"
  "${PROJECT_SOURCE_ROOT}/src/integration/sampic/collector/sampic_event_buffer.cpp"
  PROPERTIES COMPILE_OPTIONS "-O3;-march=native;-mtune=native;-ffast-math;-funroll-loops;-finline-functions"
)

set_target_properties(sampic_frontend PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  BUILD_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
  INSTALL_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
)

install(TARGETS sampic_frontend DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties(
    src/processing/sampic_processing/collector/frontend_event.cpp
    src/processing/sampic_processing/collector/modes/default/default_mode.cpp
    src/processing/sampic_processing/collector/modes/time_grouping/time_grouping_mode.cpp
    src/processing/sampic_processing/collector/modes/external_trigger/external_trigger_mode.cpp
    src/processing/sampic_processing/collector/modes/vendor_passthrough/vendor_passthrough_mode.cpp
    src/processing/sampic_processing/collector/modes/fixed_time_bins/fixed_time_bins_mode.cpp
    src/processing/sampic_processing/collector/modes/periodic_readout/periodic_readout_mode.cpp
    src/processing/sampic_processing/collector/banks/frontend_event_bank_data.cpp
    src/processing/sampic_processing/collector/banks/frontend_event_bank_advanced_hits.cpp
    src/processing/sampic_processing/collector/banks/frontend_event_bank_data_verbose.cpp
    src/processing/sampic_processing/collector/banks/frontend_event_bank_advanced_hits_verbose.cpp
    PROPERTIES COMPILE_OPTIONS "-O3")
endif()

option(SAMPIC_BUILD_THRESHOLD_SCAN "Build the supported threshold-scan worker" ON)
if(SAMPIC_BUILD_THRESHOLD_SCAN)
  add_executable(sampic_threshold_scan_worker scripts/tools/threshold_scan/worker.cpp)
  target_include_directories(sampic_threshold_scan_worker PRIVATE ${CPM_HEADER_INCLUDE_DIRS})
  target_link_libraries(sampic_threshold_scan_worker PRIVATE sampic256ch)
  target_link_options(sampic_threshold_scan_worker PRIVATE "LINKER:--disable-new-dtags")
  set_target_properties(sampic_threshold_scan_worker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    BUILD_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sampic_threshold_scan_worker PRIVATE -O3)
  endif()
endif()

option(SAMPIC_BUILD_BANK_TESTS "Build hardware-free event-bank serialization tests" OFF)
if(SAMPIC_BUILD_BANK_TESTS)
  enable_testing()
  set(SAMPIC_BANK_TEST_SOURCES
    src/processing/sampic_processing/collector/banks/frontend_event_bank_data.cpp
    src/processing/sampic_processing/collector/banks/frontend_event_bank_advanced_hits.cpp
    src/processing/sampic_processing/collector/banks/frontend_event_bank_data_verbose.cpp
    src/processing/sampic_processing/collector/banks/frontend_event_bank_advanced_hits_verbose.cpp
    src/processing/sampic_processing/collector/frontend_event.cpp
    src/integration/sampic/collector/sampic_event.cpp)
  add_executable(fixed_time_bins_test tests/fixed_time_bins_test.cpp ${SAMPIC_BANK_TEST_SOURCES}
    src/processing/sampic_processing/collector/modes/fixed_time_bins/fixed_time_bins_mode.cpp
    src/processing/sampic_processing/collector/modes/periodic_readout/periodic_readout_mode.cpp
    src/processing/sampic_processing/collector/banks/frontend_event_bank_event_timing.cpp
    src/processing/sampic_processing/collector/frontend_diagnostics.cpp
    src/processing/sampic_processing/collector/frontend_event_buffer.cpp
    src/processing/sampic_processing/collector/frontend_event_collector.cpp
    src/integration/sampic/collector/sampic_event_buffer.cpp
    src/core/threading/polling_worker.cpp)
  target_include_directories(fixed_time_bins_test PRIVATE
    "${PROJECT_SOURCE_ROOT}/include" ${CPM_HEADER_INCLUDE_DIRS})
  target_link_libraries(fixed_time_bins_test PRIVATE sampic256ch ${CPM_LINK_TARGETS} Threads::Threads)
  add_test(NAME fixed_time_bins COMMAND fixed_time_bins_test)
  add_executable(timestamp_rollover_test tests/timestamp_rollover_test.cpp ${SAMPIC_BANK_TEST_SOURCES})
  target_include_directories(timestamp_rollover_test PRIVATE "${PROJECT_SOURCE_ROOT}/include" ${CPM_HEADER_INCLUDE_DIRS})
  target_link_libraries(timestamp_rollover_test PRIVATE sampic256ch ${CPM_LINK_TARGETS} Threads::Threads)
  add_test(NAME timestamp_rollover COMMAND timestamp_rollover_test)
  add_executable(compact_event_banks_test tests/compact_event_banks_test.cpp ${SAMPIC_BANK_TEST_SOURCES})
  add_executable(compact_event_banks_benchmark tests/compact_event_banks_benchmark.cpp ${SAMPIC_BANK_TEST_SOURCES})
  target_include_directories(compact_event_banks_benchmark PRIVATE
    "${PROJECT_SOURCE_ROOT}/include" ${CPM_HEADER_INCLUDE_DIRS})
  target_link_libraries(compact_event_banks_benchmark PRIVATE sampic256ch ${CPM_LINK_TARGETS} Threads::Threads)
  target_compile_options(compact_event_banks_benchmark PRIVATE -O3)
  target_include_directories(compact_event_banks_test PRIVATE
    "${PROJECT_SOURCE_ROOT}/include" ${CPM_HEADER_INCLUDE_DIRS})
  target_link_libraries(compact_event_banks_test PRIVATE sampic256ch ${CPM_LINK_TARGETS} Threads::Threads)
  add_test(NAME compact_event_banks COMMAND compact_event_banks_test "${CMAKE_BINARY_DIR}/bank_test_fixtures")
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set_tests_properties(compact_event_banks PROPERTIES FIXTURES_SETUP bank_payloads)
  add_test(NAME compact_bank_python_decoder COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_ROOT}/tests/test_hit_bank_decoder.py" "${CMAKE_BINARY_DIR}/bank_test_fixtures")
  set_tests_properties(compact_bank_python_decoder PROPERTIES FIXTURES_REQUIRED bank_payloads)
  add_test(NAME threshold_scan_engine COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_ROOT}/tests/test_threshold_scan.py")


endif()
