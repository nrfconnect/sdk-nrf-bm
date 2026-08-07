/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Platform overrides for the default configuration settings in the
 * memfault-firmware-sdk. Default configuration settings can be found in
 * "<SDK root>/modules/lib/memfault-firmware-sdk/components/include/memfault/default_config.h"
 */

/* SoftDevice owns HardFault_Handler for vector forwarding. Use the C hook that
 * the SoftDevice forwarder calls when a fault belongs to the application.
 */
#define MEMFAULT_EXC_HANDLER_HARD_FAULT C_HardFault_Handler

/* Update the OS Name (MemfaultSdkMetric_os_name) and
 * OS Version (MemfaultSdkMetric_os_version)  to use NCS BM.
 */

#include "ncs_bare_metal_version.h"
#define MEMFAULT_OS_VERSION_NAME "ncs bm"
#define MEMFAULT_OS_VERSION_STRING NCS_BARE_METAL_VERSION_STRING
