# Copyright (c) 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

# Builds combined documentation for all documentation sets: nRF (including
# Doxygen documentation), S115, etc.
#
# We use our own Sphinx configuration files when building the documentation set
# for each repository, instead of reusing configuration files. See e.g.
# doc/nrf-bm/conf.py.

cmake_minimum_required(VERSION 3.20.0)
project(nrf-sdk-bm-doc LANGUAGES NONE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE} COMPONENTS doc)

#-------------------------------------------------------------------------------
# Options

set(SPHINXOPTS "-j auto -W --keep-going -T" CACHE STRING "Default Sphinx Options")
set(SPHINXOPTS_EXTRA "" CACHE STRING "Extra Sphinx Options")

separate_arguments(SPHINXOPTS)
separate_arguments(SPHINXOPTS_EXTRA)

#-------------------------------------------------------------------------------
# Dependencies

find_package(Python 3.10)
set(DOXYGEN_SKIP_DOT True)
find_package(Doxygen 1.12.0 REQUIRED)

find_program(SPHINXBUILD sphinx-build)
if(NOT SPHINXBUILD)
  message(FATAL_ERROR "The 'sphinx-build' command was not found")
endif()

find_program(SPHINXAUTOBUILD sphinx-autobuild)
if(NOT SPHINXAUTOBUILD)
  message(WARNING "sphinx-autobuild not found, HTML hot reloading will not be available.")
endif()

set(KCONFIG_BINARY_DIR ${CMAKE_BINARY_DIR}/kconfig)
list(INSERT MODULE_EXT_ROOT 0 ${ZEPHYR_BASE})
file(MAKE_DIRECTORY ${KCONFIG_BINARY_DIR})

#-------------------------------------------------------------------------------
# Functions

# Add a new Doxygen docset.
#
# Args:
# - name: Docset name.
# - sources: Sources.
# - version: Docset version.
# - STANDALONE: Use if docset is Doxygen-only, i.e., without Sphinx.
#
# Configured targets (if STANDALONE):
# - ${name}: Run Doxygen build.
# - ${name}-clean: Clean build artifacts.
#
# Configured targets (if NOT STANDALONE):
# - ${name}-doxygen: Run Doxygen build.
# - ${name}-doxygen-clean: Clean build artifacts.
#
function(add_doxygen_docset name sources version)
  cmake_parse_arguments(DOXYGEN "STANDALONE" "" "" ${ARGN})
  set(DOCSET_BUILD_DIR ${CMAKE_BINARY_DIR}/html/${name})
  set(DOCSET_SOURCE_BASE ${sources})
  set(DOCSET_VERSION ${version})

  if(NOT DOXYGEN_STANDALONE)
    set(SUFFIX "-doxygen")
    set(DOCSET_BUILD_DIR ${DOCSET_BUILD_DIR}/doxygen)
  endif()

  configure_file(${CMAKE_CURRENT_LIST_DIR}/${name}/${name}.doxyfile.in ${CMAKE_BINARY_DIR}/${name}.doxyfile)

  add_custom_target(
    ${name}${SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOCSET_BUILD_DIR}
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/${name}.doxyfile
    USES_TERMINAL
    COMMENT "Building ${name} Doxygen docset..."
  )

  add_custom_target(
    ${name}${SUFFIX}-clean
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${DOCSET_BUILD_DIR}
  )
endfunction()

# Add a new docset.
#
# Args:
# - name: Docset name.
# - version: Docset version.
# - DODGY: Enable/disable "dodgy" mode. If enabled "-W" (warnings as errors)
#   option will be disabled. It can be useful for external docsets that are
#   likely to generate build warnings.
# - SPHINXOPTS: Docset specific Sphinx options
#
# This function configures multiple targets which can be used to build a docset.
# The docset configuration (conf.py) is expected to be at the ${name} folder
# (relative to the current directory). The sources are taken from the
# ${name}/src folder (relative to the build directory). This means that docsets
# need to make use of the external_content extension in order to gather all
# docset sources into that folder.
#
# Configured targets:
# - ${name}: Run Sphinx "html" build.
# - ${name}-live: Run Sphinx "html" live build (if sphinx-autobuild is
#   available).
# - ${name}-linkcheck: Run Sphinx "linkcheck" target.
# - ${name}-clean: Clean build artifacts.
#
function(add_docset name version)
  cmake_parse_arguments(DOCSET "DODGY" "" "SPHINXOPTS" ${ARGN})

  set(DOCSET_CFG_DIR ${CMAKE_CURRENT_LIST_DIR}/${name})
  set(DOCSET_BUILD_DIR ${CMAKE_BINARY_DIR}/${name})
  set(DOCSET_SRC_DIR ${CMAKE_BINARY_DIR}/${name}/src)
  set(DOCSET_DOCTREE_DIR ${CMAKE_BINARY_DIR}/${name}/doctree)
  set(DOCSET_HTML_DIR ${CMAKE_BINARY_DIR}/html/${name})
  set(DOCSET_LINKCHECK_DIR ${CMAKE_BINARY_DIR}/linkcheck/${name})
  set(DOCSET_MAKE_DIRS ${DOCSET_BUILD_DIR};${DOCSET_SRC_DIR};${DOCSET_HTML_DIR})
  set(DOCSET_CLEAN_DIRS ${DOCSET_BUILD_DIR};${DOCSET_HTML_DIR})
  set(DOCSET_ENV DOXYGEN_EXECUTABLE=${DOXYGEN_EXECUTABLE};DOCSET_VERSION=${version};DOCS_HTML_DIR=${DOCSET_HTML_DIR})

  if(${DOCSET_DODGY})
    list(REMOVE_ITEM SPHINXOPTS "-W" "--keep-going")
  endif()

  add_doc_target(
    ${name}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOCSET_MAKE_DIRS}
    COMMAND ${CMAKE_COMMAND} -E env ${DOCSET_ENV}
    ${SPHINXBUILD}
      -b html
      -c ${DOCSET_CFG_DIR}
      -d ${DOCSET_DOCTREE_DIR}
      -w ${DOCSET_BUILD_DIR}/html.log
      ${SPHINXOPTS}
      ${SPHINXOPTS_EXTRA}
      ${DOCSET_SPHINXOPTS}
      ${EXTRA_ARGS}
      ${DOCSET_SRC_DIR}
      ${DOCSET_HTML_DIR}
    USES_TERMINAL
    COMMENT "Building ${name} docset..."
  )

  if(SPHINXAUTOBUILD)
    add_doc_target(
      ${name}-live
      COMMAND ${CMAKE_COMMAND} -E make_directory ${DOCSET_MAKE_DIRS}
      COMMAND ${CMAKE_COMMAND} -E env ${DOCSET_ENV}
      ${SPHINXAUTOBUILD}
        --watch ${DOCSET_CFG_DIR}
        --ignore ${DOCSET_BUILD_DIR}
        -b html
        -c ${DOCSET_CFG_DIR}
        -d ${DOCSET_DOCTREE_DIR}
        -w ${DOCSET_BUILD_DIR}/html-live.log
        ${SPHINXOPTS}
        ${SPHINXOPTS_EXTRA}
        ${DOCSET_SPHINXOPTS}
        ${EXTRA_ARGS}
        ${DOCSET_SRC_DIR}
        ${DOCSET_HTML_DIR}
      USES_TERMINAL
      COMMENT "Building ${name}-live docset..."
    )
  endif()

  add_custom_target(
    ${name}-linkcheck
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOCSET_MAKE_DIRS}
    COMMAND ${CMAKE_COMMAND} -E env ${DOCSET_ENV}
    ${SPHINXBUILD}
      -b linkcheck
      -c ${DOCSET_CFG_DIR}
      -d ${DOCSET_DOCTREE_DIR}
      -w ${DOCSET_BUILD_DIR}/linkcheck.log
      ${SPHINXOPTS}
      ${SPHINXOPTS_EXTRA}
      ${DOCSET_SPHINXOPTS}
      ${EXTRA_ARGS}
      ${DOCSET_SRC_DIR}
      ${DOCSET_LINKCHECK_DIR}
    USES_TERMINAL
    COMMENT "Checking links for ${name} docset..."
  )

  set_target_properties(
    ${name} ${name}-all
    ${name}-linkcheck
    PROPERTIES
      ADDITIONAL_CLEAN_FILES "${DOCSET_CLEAN_DIRS}"
  )

  if(SPHINXAUTOBUILD)
    set_target_properties(
      ${name}-live ${name}-live-all
      PROPERTIES
        ADDITIONAL_CLEAN_FILES "${DOCSET_CLEAN_DIRS}"
    )
  endif()

  add_custom_target(
    ${name}-clean
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${DOCSET_CLEAN_DIRS}
  )
endfunction()

# Create a custom doc target.
#
# This function has the same signature as `add_custom_target()`
#
# The function will create two targets for the doc build system.
# - Target 1 named: `<name>`
# - Target 2 named: `<name>-all`
#
# Both targets will produce same result, but target 1 is useful when only
# wanting to build a subset of the docs and missing references to other targets
# are acceptable (warnings will be generated).
#
# Target 2 is used for complete docset builds where it is important that build
# order of each target is under full control.
#
function(add_doc_target name)
  add_custom_target(${name} ${ARGN})
  add_custom_target(${name}-all ${ARGN})
endfunction()

#-------------------------------------------------------------------------------
# Paths

set(NRF_BM_BASE ${CMAKE_CURRENT_LIST_DIR}/..)

# HTML output directory
set(HTML_DIR ${CMAKE_BINARY_DIR}/html)
file(MAKE_DIRECTORY ${HTML_DIR})

#-------------------------------------------------------------------------------
# docset: nrf-bm

file(READ "${NRF_BM_BASE}/VERSION" NRF_BM_VERSION)
string(STRIP ${NRF_BM_VERSION} NRF_BM_VERSION)

add_docset(nrf-bm ${NRF_BM_VERSION})
add_doxygen_docset(nrf-bm ${NRF_BM_BASE} ${NRF_BM_VERSION})

#-------------------------------------------------------------------------------
# docset: s115

add_doxygen_docset(s115 ${NRF_BM_BASE} "9.0.0-3.prototype" STANDALONE)

#-------------------------------------------------------------------------------
# docset: kconfig

add_docset(kconfig "")

#-------------------------------------------------------------------------------
# Global targets

add_custom_target(
  copy-extra-content
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/_static/html/index.html ${HTML_DIR}
)

add_custom_target(
  merge-search-indexes
  COMMAND
    ${PYTHON_EXECUTABLE}
    ${NRF_BM_BASE}/doc/_scripts/merge_search_indexes.py
    -b ${CMAKE_BINARY_DIR}
  COMMENT "Merging search indexes..."
)

add_dependencies(merge-search-indexes
    nrf-bm-all
    kconfig-all
)

# Add dependencies to both a docset-all and docset-live-all targets if available
function(add_doc_dependencies docset)
  add_dependencies(${docset}-all ${ARGN})
  if(SPHINXAUTOBUILD)
    add_dependencies(${docset}-live-all ${ARGN})
  endif()
endfunction()

add_doc_dependencies(nrf-bm kconfig-all s115)

add_custom_target(build-all ALL)
add_dependencies(build-all
    copy-extra-content
    merge-search-indexes
    nrf-bm-all
    kconfig-all
    s115
)

add_custom_target(linkcheck)
add_dependencies(linkcheck
    nrf-bm-linkcheck
    kconfig-linkcheck
)
