#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
if(CONFIG_BM_NFC_PLATFORM)
  zephyr_library()
  zephyr_library_sources(nfc_bm_platform.c)
  zephyr_library_sources(nfc_bm_platform_internal.c)
endif()
