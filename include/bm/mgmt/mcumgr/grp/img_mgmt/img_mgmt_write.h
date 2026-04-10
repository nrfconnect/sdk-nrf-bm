/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BM_H_IMG_MGMT_WRITE_
#define BM_H_IMG_MGMT_WRITE_

/**
 * @brief MCUmgr Image Management Write API
 * @defgroup mcumgr_img_mgmt_write Image Management Write
 * @ingroup mcumgr_mgmt_api
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if there is an image write operation in progress.
 *
 * @note This should be called after @c img_mgmt_write_image_data to make sure all write operations
 *       have completed before rebooting the device.
 *
 * @retval true if there is no ongoing image write operation.
 * @retval false if an image write operation is in progress.
 */
bool img_mgmt_write_in_progress(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* BM_H_IMG_MGMT_WRITE_ */
