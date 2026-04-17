#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

function(softdevice_unity_setup)
  cmake_parse_arguments(ARG "" "VARIANT;SOC" "" ${ARGN})

  if(NOT ARG_VARIANT)
    set(ARG_VARIANT "s115")
  endif()
  if(NOT ARG_SOC)
    set(ARG_SOC "nrf54l")
  endif()

  set(SOFTDEVICE_INCLUDE_DIR
    "${ZEPHYR_NRF_BM_MODULE_DIR}/components/softdevice/${ARG_SOC}/${ARG_VARIANT}/${ARG_VARIANT}_API/include"
    PARENT_SCOPE)

  zephyr_include_directories(
    "${ZEPHYR_NRF_BM_MODULE_DIR}/components/softdevice/${ARG_SOC}/${ARG_VARIANT}/${ARG_VARIANT}_API/include")

  zephyr_compile_definitions(SVCALL_AS_NORMAL_FUNCTION)
endfunction()
