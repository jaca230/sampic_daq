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
set(SAMPIC_VENDOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/sampic_256ch_lib" CACHE PATH "Root directory of the vendor Sampic library")
if(NOT IS_DIRECTORY "${SAMPIC_VENDOR_ROOT}")
  message(FATAL_ERROR "Missing sampic_256ch_lib under ${SAMPIC_VENDOR_ROOT}. Run `git submodule update --init --recursive`.")
endif()

set(SAMPIC_VENDOR_INCLUDE_DIR "${SAMPIC_VENDOR_ROOT}/include_lib")
set(LPDEVC_INCLUDE_DIR       "${SAMPIC_VENDOR_ROOT}/lpdevc_install/include")
foreach(VENDOR_INC IN ITEMS "${SAMPIC_VENDOR_INCLUDE_DIR}" "${LPDEVC_INCLUDE_DIR}")
  if(NOT IS_DIRECTORY "${VENDOR_INC}")
    message(FATAL_ERROR "Missing vendor include directory: ${VENDOR_INC}")
  endif()
endforeach()

# Locate FTDI headers (allow user installs to override)
find_path(FTD2XX_INCLUDE_DIR
  NAMES ftd2xx.h
  PATHS "${SAMPIC_VENDOR_ROOT}/lpdevc_install/include"
  NO_DEFAULT_PATH
)
if(NOT FTD2XX_INCLUDE_DIR)
  find_path(FTD2XX_INCLUDE_DIR NAMES ftd2xx.h)
endif()
if(NOT FTD2XX_INCLUDE_DIR)
  message(FATAL_ERROR "Unable to locate ftd2xx.h. Run the vendor FTDI install script.")
endif()

set(SAMPIC_VENDOR_LIB_DIR "${SAMPIC_VENDOR_ROOT}/lib")
set(LPDEVC_LIB_DIR        "${SAMPIC_VENDOR_ROOT}/lpdevc_install/lib")
foreach(REQUIRED_LIB_NAME IN ITEMS liblpdevC.so liblpdev.so)
  if(NOT EXISTS "${LPDEVC_LIB_DIR}/${REQUIRED_LIB_NAME}")
    message(FATAL_ERROR "Missing ${REQUIRED_LIB_NAME} in ${LPDEVC_LIB_DIR}. Did you run the vendor installer?")
  endif()
endforeach()

find_library(FTD2XX_LIB_PATH
  NAMES ftd2xx libftd2xx.so libftd2xx.so.1.4.33 libftd2xx.so.1.4.8
  PATHS
    "${SAMPIC_VENDOR_ROOT}/lpdevc_install"
    "${SAMPIC_VENDOR_ROOT}/lpdevc_install/lib"
  NO_DEFAULT_PATH
)
if(NOT FTD2XX_LIB_PATH)
  find_library(FTD2XX_LIB_PATH NAMES ftd2xx)
endif()
if(NOT FTD2XX_LIB_PATH)
  message(FATAL_ERROR "Unable to locate libftd2xx.so. Ensure FTDI runtime is installed.")
endif()
get_filename_component(FTD2XX_LIB_DIR "${FTD2XX_LIB_PATH}" DIRECTORY)

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
  "${SAMPIC_VENDOR_ROOT}/sources_lib/SAMPIC_256Ch_hardware_core.c"
  "${SAMPIC_VENDOR_ROOT}/sources_lib/SAMPIC_256Ch_lib.c"
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
set_target_properties(sampic256ch PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${SAMPIC_VENDOR_LIB_DIR}"
  OUTPUT_NAME "sampic256ch"
  BUILD_RPATH "${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
  INSTALL_RPATH "${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
)

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

target_link_libraries(sampic_frontend PRIVATE
  sampic256ch
  ${CPM_LINK_TARGETS}
  Threads::Threads
  rt pthread dl util
  "${MIDASSYS_LIB_DIR}/libmfe.a"
  "${MIDASSYS_LIB_DIR}/libmidas.a"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(sampic_frontend PRIVATE -Wno-stringop-overflow -Wno-cpp)
endif()

set_target_properties(sampic_frontend PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  BUILD_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
  INSTALL_RPATH "${SAMPIC_VENDOR_LIB_DIR};${LPDEVC_LIB_DIR};${FTD2XX_LIB_DIR}"
)

if(FTD2XX_LIB_PATH)
  get_filename_component(FTD2XX_BASENAME "${FTD2XX_LIB_PATH}" NAME)
  if(FTD2XX_LIB_DIR AND NOT FTD2XX_BASENAME STREQUAL "libftd2xx.so")
    set(FTD2XX_SYMLINK "${FTD2XX_LIB_DIR}/libftd2xx.so")
    add_custom_command(
      OUTPUT "${FTD2XX_SYMLINK}"
      COMMAND ${CMAKE_COMMAND} -E create_symlink "${FTD2XX_BASENAME}" "libftd2xx.so"
      WORKING_DIRECTORY "${FTD2XX_LIB_DIR}"
      DEPENDS "${FTD2XX_LIB_PATH}"
      COMMENT "Ensuring libftd2xx.so -> ${FTD2XX_BASENAME} symlink"
      VERBATIM)
    add_custom_target(ftd2xx_symlink ALL DEPENDS "${FTD2XX_SYMLINK}")
    add_dependencies(sampic_frontend ftd2xx_symlink)
  endif()
endif()

install(TARGETS sampic_frontend DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
