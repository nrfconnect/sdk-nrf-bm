/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef NRFBM_SETTINGS_BLUETOOTH_RMEM_H__
#define NRFBM_SETTINGS_BLUETOOTH_RMEM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <bm/storage/bm_rmem.h>

/**
 * @brief Retrieve the variable that holds the desired device advertising name.
 * @param[in] ctx Run-time context of retention memory.
 * @param[out] name Pointer to store the device name pointer.
 *
 * @retval Length of the device name. 0 if value cannot be retrieved.
 */
size_t ble_name_value_get(struct bm_retained_clipboard_ctx *ctx, char const **name);

#ifdef __cplusplus
}
#endif

#endif /* NRFBM_SETTINGS_BLUETOOTH_RMEM_H__ */
