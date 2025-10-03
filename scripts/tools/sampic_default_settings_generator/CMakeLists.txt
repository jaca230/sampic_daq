cmake_minimum_required(VERSION 3.18)
project(sampic_defaults VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Check MIDAS env
if(NOT DEFINED ENV{MIDASSYS})
  message(FATAL_ERROR "MIDASSYS not set – source MIDAS env first.")
endif()

set(MIDASSYS_INCLUDE_DIRS
  $ENV{MIDASSYS}/include
  $ENV{MIDASSYS}/mxml
  $ENV{MIDASSYS}/midasio
  $ENV{MIDASSYS}/mjson
)
set(MIDASSYS_LIB_DIR $ENV{MIDASSYS}/lib)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/CPM.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/CPMConfig.cmake)

add_executable(sampic_defaults src/main.cpp)

target_include_directories(sampic_defaults PRIVATE
  ${MIDASSYS_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

target_link_libraries(sampic_defaults PRIVATE
  sampic256ch
  lpdevC
  lpdev
  ftd2xx
  rt pthread dl util
  ${MIDASSYS_LIB_DIR}/libmfe.a
  ${MIDASSYS_LIB_DIR}/libmidas.a
)

set_target_properties(sampic_defaults PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
