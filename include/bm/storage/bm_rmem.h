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

/**
 * @brief Run-time context for the retained clipboard instance.
 *
 * @param offset Offset of the current entry.
 * @param max_offset Maximum offset within the retained clipboard.
 */
struct bm_retained_clipboard_ctx {
	uint16_t offset;
	uint16_t max_offset;
};

/**
 * @brief Data descriptor for the retained clipboard entry.
 *
 * @param[in] type Type of the data.
 * @param[in] len Length of the data.
 * @param[out] data Pointer to the data.
 */
struct bm_rmem_data_desc {
	uint16_t type;
	uint16_t len;
	void *data;
};

/**
 * @brief Initialize the retained clipboard writer.
 *
 * @param[in,out] ctx Run-time context for the retained clipboard instance.
 * @retval 0 on success, negative error code on failure.
 */
int bm_rmem_writer_init(struct bm_retained_clipboard_ctx *ctx);

/**
 * @brief Write data to the retained clipboard.
 *
 * @param[in,out] ctx Run-time context for the retained clipboard instance.
 * @param[in] type Type of the data.
 * @param[in] data Pointer to the data.
 * @param[in] len Length of the data.
 * @retval 0 on success, negative error code on failure.
 */
int bm_rmem_data_write(struct bm_retained_clipboard_ctx *ctx, uint16_t type, const void *data,
		       uint16_t len);

/**
 * @brief Write CRC32 to the retained clipboard.
 * Such CRC covers all the data written to the retained clipboard.
 *
 * @param[in,out] ctx Run-time context for the retained clipboard instance.
 * @retval 0 on success, negative error code on failure.
 */
int bm_rmem_crc32_write(struct bm_retained_clipboard_ctx *ctx);

/**
 * @brief Verify the CRC32 of the retained clipboard.
 *
 * @retval If CRC valid, length of content coverde by it, negative error code on failure.
 */
int bm_rmem_crc32_verify(void);

/**
 * @brief Get data from the retained clipboard.
 * The caller must populate decs.type to data TLV type to retrieve.
 *
 * @param[in,out] ctx Run-time context for the retained clipboard instance.
 * @param[in,out] desc Data descriptor. On success, desc.data points to the data,
 *                     desc.len is set to the length of the data.
 * @retval 0 on success, negative error code on failure.
 */
int bm_rmem_data_get(struct bm_retained_clipboard_ctx *ctx, struct bm_rmem_data_desc *desc);

/**
 * @brief Initialize the retained clipboard reader.
 *
 * @param[in,out] ctx Run-time context for the retained clipboard instance.
 * @retval 0 on success, negative error code on failure.
 */
int bm_rmem_reader_init(struct bm_retained_clipboard_ctx *ctx);

/**
 * @brief Clear the retained clipboard.
 *
 * @retval 0 on success, negative error code on failure.
 */
int bm_rmem_clear(void);

#define BM_REM_TLV_TYPE_BLE_NAME 0x0001
#define BM_REM_TLV_TYPE_CRC_32 0x0002

#ifdef __cplusplus
}
#endif

#endif /* NRFBM_STORAGE_RMEM_H__ */
