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

/* Run-time context for the retained clipboard instance*/
struct bm_retained_clipboard_ctx {
	uint16_t offset;
	uint16_t max_offset;
};

struct bm_rmem_data_desc {
	uint16_t type;
	uint16_t len;
	void *data;
};

int bm_rmem_init_writer(struct bm_retained_clipboard_ctx *ctx);
int bm_rmem_write_data(struct bm_retained_clipboard_ctx *ctx, uint16_t type, const void *data, uint16_t len);
int bm_rmem_write_crc32(struct bm_retained_clipboard_ctx *ctx);
int bm_rmem_verify_crc32(void);
int bm_rmem_get_data(struct bm_retained_clipboard_ctx *ctx, struct bm_rmem_data_desc *desc);
int bm_rmem_init_reader(struct bm_retained_clipboard_ctx *ctx);
int bm_rmem_get_data(struct bm_retained_clipboard_ctx *ctx, struct bm_rmem_data_desc *desc);

#define BM_REM_TLV_TYPE_BLE_NAME 0x0001
#define BM_REM_TLV_TYPE_CRC_32 0x0002

#ifdef __cplusplus
}
#endif

#endif