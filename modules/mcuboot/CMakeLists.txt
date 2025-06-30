#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

include(${ZEPHYR_NRF_MODULE_DIR}/modules/mcuboot/CMakeLists.txt)

if(CONFIG_MCUBOOT AND CONFIG_NCS_BM)
  include(${ZEPHYR_NRF_BM_MODULE_DIR}/cmake/image_signing_softdevice.cmake)
  include(${ZEPHYR_NRF_BM_MODULE_DIR}/cmake/flash_metadata.cmake)
endif()
