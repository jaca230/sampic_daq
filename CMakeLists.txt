cmake_minimum_required(VERSION 3.18)
project(sampic_frontend VERSION 1.0 LANGUAGES CXX)

# --------------------------------------------------------------------------
# Enforce C++20
# --------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------------------------------
# Check MIDAS environment
# --------------------------------------------------------------------------
if(NOT DEFINED ENV{MIDASSYS})
  message(FATAL_ERROR "MIDASSYS environment variable not set. Source MIDAS environment first.")
endif()

set(MIDASSYS_INCLUDE_DIRS
  $ENV{MIDASSYS}/include
  $ENV{MIDASSYS}/mxml
  $ENV{MIDASSYS}/midasio
  $ENV{MIDASSYS}/mjson
)
set(MIDASSYS_LIB_DIR $ENV{MIDASSYS}/lib)

# --------------------------------------------------------------------------
# CPM Setup
# --------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPMConfig.cmake)

# --------------------------------------------------------------------------
# Fetch and configure CPM packages
# --------------------------------------------------------------------------
foreach(pkg IN LISTS CPM_PACKAGE_LIST)

  # Default DOWNLOAD_ONLY to NO if not defined
  if(NOT DEFINED ${pkg}_DOWNLOAD_ONLY)
    set(${pkg}_DOWNLOAD_ONLY NO)
  endif()

  if(DEFINED ${pkg}_URL)
    CPMFindPackage(
      NAME ${pkg}
      URL ${${pkg}_URL}
      GIT_TAG ${${pkg}_TAG}
      DOWNLOAD_ONLY ${${pkg}_DOWNLOAD_ONLY}
      OPTIONS ${${pkg}_OPTIONS}
    )
  elseif(DEFINED ${pkg}_REPO)
    if(${${pkg}_REPO} MATCHES "^(git@|https://)")
      CPMFindPackage(
        NAME ${pkg}
        GIT_REPOSITORY ${${pkg}_REPO}
        GIT_TAG ${${pkg}_TAG}
        DOWNLOAD_ONLY ${${pkg}_DOWNLOAD_ONLY}
        OPTIONS ${${pkg}_OPTIONS}
      )
    else()
      CPMFindPackage(
        NAME ${pkg}
        GITHUB_REPOSITORY ${${pkg}_REPO}
        GIT_TAG ${${pkg}_TAG}
        DOWNLOAD_ONLY ${${pkg}_DOWNLOAD_ONLY}
        OPTIONS ${${pkg}_OPTIONS}
      )
    endif()
  else()
    message(FATAL_ERROR "Neither URL nor REPO defined for package ${pkg}")
  endif()

  # If header-only, create interface target
  if(${${pkg}_DOWNLOAD_ONLY})
    if(NOT TARGET ${pkg}_header_only)
      add_library(${pkg}_header_only INTERFACE)
      target_include_directories(${pkg}_header_only INTERFACE
        $<BUILD_INTERFACE:${${pkg}_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
      )
      add_library(${pkg}::${pkg} ALIAS ${pkg}_header_only)
      set(${pkg}_TARGET ${pkg}::${pkg})
    endif()
  endif()
endforeach()

# --------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------
# Top-level main file
set(MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/frontend.cpp)

# All other cpp files in src/ and subdirectories
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Add executable
add_executable(sampic_frontend ${MAIN_SRC} ${SRC_FILES})

# --------------------------------------------------------------------------
# Include directories
# --------------------------------------------------------------------------
target_include_directories(sampic_frontend PRIVATE
  ${MIDASSYS_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --------------------------------------------------------------------------
# Link system & hardware libraries
# --------------------------------------------------------------------------
target_link_libraries(sampic_frontend PRIVATE
  sampic256ch
  lpdevC
  lpdev
  ftd2xx
  rt
  pthread
  dl
  util
  ${MIDASSYS_LIB_DIR}/libmfe.a
  ${MIDASSYS_LIB_DIR}/libmidas.a
)

# --------------------------------------------------------------------------
# Link all CPM targets
# --------------------------------------------------------------------------
foreach(pkg IN LISTS CPM_PACKAGE_LIST)
  if(DEFINED ${pkg}_TARGET AND NOT ${${pkg}_TARGET} STREQUAL "")
    target_link_libraries(sampic_frontend PRIVATE ${${pkg}_TARGET})
  elseif(DEFINED ${pkg}_TARGETS)
    foreach(subtarget IN LISTS ${pkg}_TARGETS)
      target_link_libraries(sampic_frontend PRIVATE ${subtarget})
    endforeach()
  else()
    message(STATUS "Skipping header-only or no-target package: ${pkg}")
  endif()
endforeach()

# --------------------------------------------------------------------------
# Compiler options
# --------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(sampic_frontend PRIVATE -Wno-stringop-overflow -Wno-cpp)
endif()

# --------------------------------------------------------------------------
# RPATH setup
# --------------------------------------------------------------------------
set_target_properties(sampic_frontend PROPERTIES
  BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/lib:$ENV{LD_LIBRARY_PATH}"
)

# --------------------------------------------------------------------------
# Installation
# --------------------------------------------------------------------------
install(TARGETS sampic_frontend DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
