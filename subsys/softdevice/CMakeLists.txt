#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_SOFTDEVICE_S115 OR CONFIG_SOFTDEVICE_S145)
  zephyr_include_directories("${ZEPHYR_NRF_BM_MODULE_DIR}/components/softdevice/${CONFIG_SOFTDEVICE_SOC_VARIANT_STR}/${CONFIG_SOFTDEVICE_VARIANT_STR}/${CONFIG_SOFTDEVICE_VARIANT_STR}_API/include")
endif()

# SVCALL_AS_NORMAL_FUNCTION makes BLE API headers declare normal functions (no ARM SVC asm).
if(CONFIG_SOFTDEVICE_BSIM)
  zephyr_compile_definitions(SVCALL_AS_NORMAL_FUNCTION)
endif()
