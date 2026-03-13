/**
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @brief Backend API declarations for the NCS Bare Metal Storage library.
 *
 * This header declares the available storage backend API instances based on
 * Kconfig selection.  It is included transitively by
 * @ref bm_storage.h so that consumers do not need to add a separate include.
 */

#ifndef BM_STORAGE_BACKENDS_H__
#define BM_STORAGE_BACKENDS_H__

#include <bm/storage/bm_storage.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_BM_STORAGE_BACKEND_SD)
extern const struct bm_storage_api bm_storage_sd_api;
#endif

#if defined(CONFIG_BM_STORAGE_BACKEND_RRAM)
extern const struct bm_storage_api bm_storage_rram_api;
#endif

#if defined(CONFIG_BM_STORAGE_BACKEND_NATIVE_SIM)
extern const struct bm_storage_api bm_storage_native_sim_api;
#endif

#ifdef __cplusplus
}
#endif

#endif /* BM_STORAGE_BACKENDS_H__ */
