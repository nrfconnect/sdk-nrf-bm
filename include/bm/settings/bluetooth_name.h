/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>

/**
 * @brief Retrieve the variable that holds the desired device advertising name.
 *
 * @retval Address of device name setting.
 */
const char *bluetooth_name_value_get(void);
