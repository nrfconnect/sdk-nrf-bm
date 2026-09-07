#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_BM_MEMFAULT_LOCK)
  # Default implementations of memfault_lock() and memfault_unlock() use zephyr mutexes.
  # With multithreading disabled, mutexes are not available and a custom implementation
  # with IRQ locking is needed.
  # Override memfault_platform_lock.c with memfault_platform_lock_bm.c.

  # Trick to be able to replace a source file.
  set_source_files_properties(
    ${ZEPHYR_MEMFAULT_FIRMWARE_SDK_MODULE_DIR}/ports/zephyr/common/memfault_platform_lock.c
    DIRECTORY ${ZEPHYR_MEMFAULT_FIRMWARE_SDK_MODULE_DIR}/ports/zephyr
    PROPERTIES HEADER_FILE_ONLY ON
  )

  set(ZEPHYR_CURRENT_LIBRARY "memfault-firmware-sdk")
  zephyr_library_sources(memfault_platform_lock_bm.c)
endif()
