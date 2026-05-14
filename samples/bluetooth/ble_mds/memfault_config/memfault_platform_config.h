/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 *
 * Platform overrides for the default configuration settings in the
 * memfault-firmware-sdk. Default configuration settings can be found in
 * "<NCS folder>/modules/lib/memfault-firmware-sdk/components/include/memfault/default_config.h"
 */

/* SoftDevice owns HardFault_Handler for vector forwarding. Use the C hook that
 * the SoftDevice forwarder calls when a fault belongs to the application.
 */
#define MEMFAULT_EXC_HANDLER_HARD_FAULT C_HardFault_Handler
