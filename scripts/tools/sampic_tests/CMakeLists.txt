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

add_executable(sampic_pulser_rate
  src/sampic_pulser_rate.cpp)

add_executable(sampic_crate_smoke
  src/sampic_crate_smoke.cpp)

target_include_directories(sampic_pulser_rate PRIVATE
  ${SAMPIC_VENDOR_INCLUDE}
  ${LPDEVC_INCLUDE_DIR})

target_include_directories(sampic_crate_smoke PRIVATE
  ${SAMPIC_VENDOR_INCLUDE}
  ${LPDEVC_INCLUDE_DIR})

set(SAMPIC_VENDOR_LIBS
  ${SAMPIC_VENDOR_LIB_DIR}/libsampic256ch.so
  ${LPDEVC_LIB_DIR}/liblpdevC.so
  ${LPDEVC_LIB_DIR}/liblpdev.so
  ${FTD2XX_LIB_PATH})

foreach(target sampic_pulser_rate sampic_crate_smoke)
  target_link_libraries(${target} PRIVATE
    ${SAMPIC_VENDOR_LIBS}
    Threads::Threads
    dl rt pthread m)
endforeach()

set_target_properties(sampic_pulser_rate PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  BUILD_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
  INSTALL_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}")

set_target_properties(sampic_crate_smoke PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  BUILD_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
  INSTALL_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}")

install(TARGETS sampic_pulser_rate sampic_crate_smoke RUNTIME DESTINATION bin)
