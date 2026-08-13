# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bleberry LANGUAGES C)

# --- Locate the Berry interpreter (fetched by west, see west.yml) ------------

if(NOT DEFINED WEST_TOPDIR)
  get_filename_component(WEST_TOPDIR ${ZEPHYR_BASE}/.. ABSOLUTE)
endif()

set(BERRY_DIR ${WEST_TOPDIR}/modules/lib/berry CACHE PATH
    "Path to the Berry language source tree")

if(NOT EXISTS ${BERRY_DIR}/src/berry.h)
  message(FATAL_ERROR
    "Berry sources not found at ${BERRY_DIR}. Run 'west update' first, "
    "or set BERRY_DIR to a Berry checkout.")
endif()

set(BERRY_CONF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/berry_conf)
set(BERRY_GENERATE_DIR ${BERRY_DIR}/generate)

file(GLOB BERRY_SOURCES ${BERRY_DIR}/src/*.c)
file(GLOB BERRY_HEADERS ${BERRY_DIR}/src/*.h)

# --- Berry pre-build code generation ("coc") ----------------------------------
# Berry solidifies its built-in classes/modules into flash. The coc tool scans
# the sources (plus our app sources, so custom const objects work too) and
# emits the constant tables into ${BERRY_DIR}/generate, where the Berry
# sources expect to find them.

add_custom_command(
  OUTPUT ${BERRY_GENERATE_DIR}/be_const_strtab.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${BERRY_GENERATE_DIR}
  COMMAND ${PYTHON_EXECUTABLE} ${BERRY_DIR}/tools/coc/coc
          -o ${BERRY_GENERATE_DIR}
          ${BERRY_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/src
          -c ${BERRY_CONF_DIR}/berry_conf.h
  DEPENDS ${BERRY_SOURCES} ${BERRY_HEADERS} ${BERRY_CONF_DIR}/berry_conf.h
  COMMENT "Berry: generating precompiled objects (coc)"
)
add_custom_target(berry_coc DEPENDS ${BERRY_GENERATE_DIR}/be_const_strtab.h)

# --- Firmware git hash (regenerated on every build) ---------------------------

set(BLEBERRY_GIT_H ${CMAKE_BINARY_DIR}/bleberry_git.h)
add_custom_target(bleberry_git_hash
  COMMAND ${CMAKE_COMMAND}
          -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
          -DOUT=${BLEBERRY_GIT_H}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/git_hash.cmake
  BYPRODUCTS ${BLEBERRY_GIT_H}
  COMMENT "Updating bleberry_git.h"
)

# --- Application ---------------------------------------------------------------
# The Berry core is compiled straight into the app target: zephyr_library()
# is not available to application CMakeLists (Zephyr application mode).

target_sources(app PRIVATE
  ${BERRY_SOURCES}
  src/main.c
  src/nus_io.c
  src/berry_repl.c
  src/berry_io.c
  src/bb_runtime.c
  src/berry_port.c
  src/be_modtab.c
)
target_include_directories(app PRIVATE
  src
  ${BERRY_DIR}/src
  ${BERRY_CONF_DIR}
  ${CMAKE_BINARY_DIR}
)
add_dependencies(app berry_coc bleberry_git_hash)
