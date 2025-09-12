cmake_minimum_required(VERSION 3.10)

# Project name and version
project(sampic_frontend VERSION 1.0 LANGUAGES CXX C)

# Check if MIDASSYS environment variable is defined
if (NOT DEFINED ENV{MIDASSYS})
    message(FATAL_ERROR "MIDASSYS environment variable not defined.")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Home directory relative to CMakeLists.txt
set(HOMEDIR "${CMAKE_SOURCE_DIR}/..")

# Include directories
set(INCLUDE_DIR "${HOMEDIR}/include_lib")
include_directories(
    ${INCLUDE_DIR}
    $ENV{MIDASSYS}/include
    $ENV{MIDASSYS}/mxml
    $ENV{MIDASSYS}/midasio
    $ENV{MIDASSYS}/mjson
)

# Source files
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")
set(SOURCES "${SOURCE_DIR}/frontend.cpp")  # explicitly include frontend.cpp

# Add executable
add_executable(sampic_frontend ${SOURCES})

# Library search directories
link_directories(
    "${HOMEDIR}/lib"
    $ENV{MIDASSYS}/lib
)

# Link libraries
target_link_libraries(sampic_frontend PRIVATE
    sampic256ch   # SAMPIC library
    lpdevC        # Low-level hardware libraries
    lpdev
    ftd2xx
    rt
    pthread
    dl
    util
    $ENV{MIDASSYS}/lib/libmfe.a
    $ENV{MIDASSYS}/lib/libmidas.a
)

# RPATH so it can find libsampic256ch at runtime
set_target_properties(sampic_frontend PROPERTIES
    BUILD_RPATH "${HOMEDIR}/lib:$ENV{LD_LIBRARY_PATH}"
)

# Installation directory
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}")
install(TARGETS sampic_frontend DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
