/**
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRFBM_STORAGE_RMEM_H__
#define NRFBM_STORAGE_RMEM_H__

#ifdef __cplusplus
extern "C" {
#endif

#define BM_RMEM_TLV_TYPE_BLE_NAME 0x0001
#define BM_RMEM_TLV_TYPE_CRC_32 0x0002

/**
 * @brief Runtime context for the retained clipboard instance.
 */
struct bm_retained_clipboard_ctx {
	uint16_t offset;      /**< Offset of the current entry. */
	uint16_t max_offset;  /**< Maximum offset within the retained clipboard. */
};

/**
 * @brief Data descriptor for the retained clipboard entry.
 */
struct bm_rmem_data_desc {
	uint16_t type;        /**< Type of the data. */
	uint16_t len;         /**< Length of the data. */
	void *data;           /**< Pointer to the data. */
};

/**
 * @brief Write data to the retained clipboard.
 *
 * @param[in,out] ctx Run-time context for the retained clipboard instance.
 * @param[in] type Type of the data.
 * @param[in] data Pointer to the data.
 * @param[in] len Length of the data.
 * @retval 0 on success.
 * @return Negative error code on failure.
 */
int bm_rmem_data_write(struct bm_retained_clipboard_ctx *ctx, uint16_t type, const void *data,
		       uint16_t len);

/**
 * @brief Write CRC32 to the retained clipboard.
 * Such CRC covers all the data written to the retained clipboard.
 *
 * @param[in,out] ctx Run-time context for the retained clipboard instance.
 * @retval 0 on success.
 * @return Negative error code on failure.
 */
int bm_rmem_crc32_write(struct bm_retained_clipboard_ctx *ctx);

/**
 * @brief Verify the CRC32 of the retained clipboard.
 *
 * @retval 0 or positive length of content covered by CRC when valid.
 *         Negative error code on failure.
 */
int bm_rmem_crc32_verify(void);

/**
 * @brief Get data from the retained clipboard.
 * The caller must set desc.type to the TLV type of the entry to retrieve.
 *
 * @param[in,out] ctx Runtime context for the retained clipboard instance.
 * @param[in,out] desc Data descriptor. On success, desc.data points to the data,
 *                     desc.len is set to the length of the data.
 * @retval 0 on success.
 * @return Negative error code on failure.
 */
int bm_rmem_data_get(struct bm_retained_clipboard_ctx *ctx, struct bm_rmem_data_desc *desc);

/**
 * @brief Initialize the retained clipboard.
 *
 * @param[in,out] ctx Runtime context for the retained clipboard instance.
 * @retval 0 on success.
 */
int bm_rmem_init(struct bm_retained_clipboard_ctx *ctx);

/**
 * @brief Initialize the retained clipboard writer.
 *
 * Similar to @ref bm_rmem_init but disregards physical clipboard content
 * and initializes @ctx for writing to empty storage.
 *
 * @param[in,out] ctx Run-time context for the retained clipboard instance.
 * @retval 0 on success.
 */
int bm_rmem_init_empty(struct bm_retained_clipboard_ctx *ctx);

/**
 * @brief Clear the retained clipboard.
 *
 * @p ctx is updated by this function so it reflects that storage is empty.
 *
 * @param[in,out] ctx Run-time context for the retained clipboard instance.
 *
 * @retval 0 on success.
 */
int bm_rmem_clear(struct bm_retained_clipboard_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* NRFBM_STORAGE_RMEM_H__ */
