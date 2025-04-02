file(STRINGS ${ZEPHYR_NRF_LITE_MODULE_DIR}/VERSION NCS_BARE_METAL_VERSION LIMIT_COUNT 1 LENGTH_MINIMUM 5)
string(REGEX MATCH "([^\.]*)\.([^\.]*)\.([^-]*)[-]?(.*)" OUT_VAR ${NCS_BARE_METAL_VERSION})

set(NCS_BARE_METAL_VERSION_MAJOR ${CMAKE_MATCH_1})
set(NCS_BARE_METAL_VERSION_MINOR ${CMAKE_MATCH_2})
set(NCS_BARE_METAL_VERSION_PATCH ${CMAKE_MATCH_3})
set(NCS_BARE_METAL_VERSION_EXTRA ${CMAKE_MATCH_4})

math(EXPR NCS_BARE_METAL_VERSION_CODE
  "(${NCS_BARE_METAL_VERSION_MAJOR} << 16) + (${NCS_BARE_METAL_VERSION_MINOR} << 8)  + (${NCS_BARE_METAL_VERSION_PATCH})")

math(EXPR NCS_BARE_METAL_VERSION_NUMBER
  ${NCS_BARE_METAL_VERSION_CODE} OUTPUT_FORMAT HEXADECIMAL)

add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/include/generated/ncs_bare_metal_version.h
  COMMAND ${CMAKE_COMMAND} -DZEPHYR_BASE=${ZEPHYR_BASE}
    -DOUT_FILE=${PROJECT_BINARY_DIR}/include/generated/ncs_bare_metal_version.h
    -DVERSION_TYPE=NCS_BARE_METAL
    # We don't want Zephyr to parse the NCS Bare Metal VERSION file as it is not conforming
    # to Zephyr VERSION format. Pointing to a non-existing file allows us full control from
    # CMake instead.
    -DVERSION_FILE=${ZEPHYR_NRF_LITE_MODULE_DIR}/VERSIONFILEDUMMY
    -DNCS_BARE_METAL_VERSION_CODE=${NCS_BARE_METAL_VERSION_CODE}
    -DNCS_BARE_METAL_VERSION_NUMBER=${NCS_BARE_METAL_VERSION_NUMBER}
    -DNCS_BARE_METAL_VERSION_MAJOR=${NCS_BARE_METAL_VERSION_MAJOR}
    -DNCS_BARE_METAL_VERSION_MINOR=${NCS_BARE_METAL_VERSION_MINOR}
    -DNCS_BARE_METAL_PATCHLEVEL=${NCS_BARE_METAL_VERSION_PATCH}
    -DNCS_BARE_METAL_VERSION_STRING=${NCS_BARE_METAL_VERSION}
    -P ${ZEPHYR_BASE}/cmake/gen_version_h.cmake
    DEPENDS ${ZEPHYR_NRF_LITE_MODULE_DIR}/VERSION ${git_dependency}
)
add_custom_target(ncs_bare_metal_version_h DEPENDS ${PROJECT_BINARY_DIR}/include/generated/ncs_bare_metal_version.h)
add_dependencies(version_h ncs_bare_metal_version_h)

find_package(Git QUIET)
if(GIT_FOUND)
  # The gitdir folder will not be under ZEPHYR_NRF_LITE_MODULE_DIR when using git worktrees.
  # Use git rev-parse to find the absolute path to the git directory.
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --absolute-git-dir
    WORKING_DIRECTORY                ${ZEPHYR_NRF_LITE_MODULE_DIR}
    OUTPUT_VARIABLE                  ABSOLUTE_GIT_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE                   stderr
    RESULT_VARIABLE                  return_code
  )

  if(return_code)
    message(STATUS "git rev-parse failed: ${stderr}")
  elseif(NOT "${stderr}" STREQUAL "")
    message(STATUS "git rev-parse warned: ${stderr}")
  endif()
else()
  set(ABSOLUTE_GIT_DIR ${ZEPHYR_NRF_LITE_MODULE_DIR}/".git")
endif()

add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/include/generated/ncs_bare_metal_commit.h
  COMMAND ${CMAKE_COMMAND} -DZEPHYR_BASE=${ZEPHYR_BASE} -DNRF_DIR=${NRF_DIR}
    -DOUT_FILE=${PROJECT_BINARY_DIR}/include/generated/ncs_bare_metal_commit.h
    -DCOMMIT_TYPE=NCS_BARE_METAL
    -DCOMMIT_PATH=${ZEPHYR_NRF_LITE_MODULE_DIR}
    -P ${ZEPHYR_NRF_MODULE_DIR}/cmake/gen_commit_h.cmake
    DEPENDS ${ZEPHYR_NRF_LITE_MODULE_DIR}/VERSION ${ABSOLUTE_GIT_DIR} ${ABSOLUTE_GIT_DIR}/index
)
add_custom_target(ncs_bare_metal_commit_h DEPENDS ${PROJECT_BINARY_DIR}/include/generated/ncs_bare_metal_commit.h)
add_dependencies(version_h ncs_bare_metal_commit_h)
