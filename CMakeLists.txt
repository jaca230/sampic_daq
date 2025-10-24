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
# Local SAMPIC dependency
# --------------------------------------------------------------------------
set(SAMPIC_LIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/external/sampic_256ch_lib)
if(NOT EXISTS ${SAMPIC_LIB_ROOT})
  message(FATAL_ERROR "Missing sampic_256ch_lib submodule. Run `git submodule update --init --recursive`.")
endif()

set(SAMPIC_LIB_INCLUDE_DIR ${SAMPIC_LIB_ROOT}/include_lib)
set(LPDEVC_INCLUDE_DIR ${SAMPIC_LIB_ROOT}/lpdevc_install/include)
set(SAMPIC_LIB_DIR ${SAMPIC_LIB_ROOT}/lib)
set(LPDEVC_LIB_DIR ${SAMPIC_LIB_ROOT}/lpdevc_install/lib)
set(FTD2XX_INCLUDE_DIR ${SAMPIC_LIB_ROOT}/lpdevc_install/include)

if(NOT EXISTS ${LPDEVC_LIB_DIR}/liblpdevC.so)
  message(FATAL_ERROR "Expected liblpdevC.so in ${LPDEVC_LIB_DIR}. Ensure the sampic_256ch_lib submodule is populated.")
endif()

if(NOT EXISTS ${LPDEVC_LIB_DIR}/liblpdev.so)
  message(FATAL_ERROR "Expected liblpdev.so in ${LPDEVC_LIB_DIR}. Ensure the sampic_256ch_lib submodule is populated.")
endif()

find_library(FTD2XX_LIB_PATH
  NAMES ftd2xx libftd2xx.so libftd2xx.so.1.4.33 libftd2xx.so.1.4.8
  PATHS
    ${SAMPIC_LIB_ROOT}/lpdevc_install
    ${SAMPIC_LIB_ROOT}/lpdevc_install/lib
  NO_DEFAULT_PATH
)
if(NOT FTD2XX_LIB_PATH)
  find_library(FTD2XX_LIB_PATH NAMES ftd2xx)
endif()
if(NOT FTD2XX_LIB_PATH)
  message(FATAL_ERROR "Unable to locate FTDI (libftd2xx) library. Expected under ${SAMPIC_LIB_ROOT}/lpdevc_install.")
endif()
get_filename_component(FTD2XX_LIB_DIR ${FTD2XX_LIB_PATH} DIRECTORY)

set(SAMPIC_LIB_CFLAGS "-D__LINUX_BUILD -fPIC -I${SAMPIC_LIB_INCLUDE_DIR} -I${LPDEVC_INCLUDE_DIR} -I${FTD2XX_INCLUDE_DIR} -Wall")
set(SAMPIC_LIB_LFLAGS "-L${LPDEVC_LIB_DIR} -L${FTD2XX_LIB_DIR} -llpdevC -llpdev -lftd2xx -lrt -lpthread -m64 -Wl,-rpath,${SAMPIC_LIB_DIR} -Wl,-rpath,${LPDEVC_LIB_DIR} -Wl,-rpath,${FTD2XX_LIB_DIR}")

find_program(SAMPIC_LIB_BUILD_TOOL NAMES gmake make)
if(NOT SAMPIC_LIB_BUILD_TOOL)
  message(FATAL_ERROR "Unable to find 'make' required to build sampic_256ch_lib.")
endif()

file(GLOB_RECURSE SAMPIC_LIB_SOURCE_FILES
  ${SAMPIC_LIB_ROOT}/sources_lib/*.* 
  ${SAMPIC_LIB_ROOT}/include_lib/*.* 
  ${SAMPIC_LIB_ROOT}/Makefile
)

add_custom_command(
  OUTPUT ${SAMPIC_LIB_DIR}/libsampic256ch.so
  COMMAND ${CMAKE_COMMAND} -E env
    "CFLAGS=${SAMPIC_LIB_CFLAGS}"
    "LFLAGS=${SAMPIC_LIB_LFLAGS}"
    "USRLOCLIBDIR=${SAMPIC_LIB_DIR}"
    "USRLIBDIR=${SAMPIC_LIB_DIR}"
    "LIBDIR=${SAMPIC_LIB_DIR}"
    ${SAMPIC_LIB_BUILD_TOOL} lib
  WORKING_DIRECTORY ${SAMPIC_LIB_ROOT}
  COMMENT "Building sampic_256ch_lib (libsampic256ch.so)"
  VERBATIM
  DEPENDS ${SAMPIC_LIB_SOURCE_FILES}
)

add_custom_target(sampic_256ch_lib_build
  DEPENDS ${SAMPIC_LIB_DIR}/libsampic256ch.so
)

add_library(sampic256ch SHARED IMPORTED GLOBAL)
add_library(sampic256ch::sampic256ch ALIAS sampic256ch)
set_target_properties(sampic256ch PROPERTIES
  IMPORTED_LOCATION ${SAMPIC_LIB_DIR}/libsampic256ch.so
  INTERFACE_INCLUDE_DIRECTORIES "${SAMPIC_LIB_INCLUDE_DIR};${FTD2XX_INCLUDE_DIR}"
)
add_dependencies(sampic256ch sampic_256ch_lib_build)

add_library(lpdevC SHARED IMPORTED GLOBAL)
add_library(lpdev::lpdevC ALIAS lpdevC)
set_target_properties(lpdevC PROPERTIES
  IMPORTED_LOCATION ${LPDEVC_LIB_DIR}/liblpdevC.so
  INTERFACE_INCLUDE_DIRECTORIES "${LPDEVC_INCLUDE_DIR};${FTD2XX_INCLUDE_DIR}"
)
add_dependencies(lpdevC sampic_256ch_lib_build)

add_library(lpdev SHARED IMPORTED GLOBAL)
add_library(lpdev::lpdev ALIAS lpdev)
set_target_properties(lpdev PROPERTIES
  IMPORTED_LOCATION ${LPDEVC_LIB_DIR}/liblpdev.so
  INTERFACE_INCLUDE_DIRECTORIES "${LPDEVC_INCLUDE_DIR};${FTD2XX_INCLUDE_DIR}"
)
add_dependencies(lpdev sampic_256ch_lib_build)

add_library(ftd2xx SHARED IMPORTED GLOBAL)
add_library(ftd2xx::ftd2xx ALIAS ftd2xx)
set_target_properties(ftd2xx PROPERTIES
  IMPORTED_LOCATION ${FTD2XX_LIB_PATH}
  INTERFACE_INCLUDE_DIRECTORIES ${FTD2XX_INCLUDE_DIR}
)

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
add_dependencies(sampic_frontend sampic_256ch_lib_build)

# --------------------------------------------------------------------------
# Include directories
# --------------------------------------------------------------------------
target_include_directories(sampic_frontend PRIVATE
  ${MIDASSYS_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${SAMPIC_LIB_INCLUDE_DIR}
  ${LPDEVC_INCLUDE_DIR}
  ${FTD2XX_INCLUDE_DIR}
)

# --------------------------------------------------------------------------
# Link system & hardware libraries
# --------------------------------------------------------------------------
target_link_libraries(sampic_frontend PRIVATE
  sampic256ch::sampic256ch
  lpdev::lpdevC
  lpdev::lpdev
  ftd2xx::ftd2xx
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
  BUILD_RPATH "${SAMPIC_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
  INSTALL_RPATH "${SAMPIC_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
)

# --------------------------------------------------------------------------
# Installation
# --------------------------------------------------------------------------
install(TARGETS sampic_frontend DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
