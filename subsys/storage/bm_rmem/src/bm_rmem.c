/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */



#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <string.h>
#include <errno.h>
#include <bm/storage/bm_rmem.h>

#define DT_DRV_COMPAT zephyr_retained_ram

LOG_MODULE_REGISTER(bm_rmem_clipboard, CONFIG_NCS_BM_RMEM_CLIPBOARD_LOG_LEVEL);

struct bm_retained_clipboard_inst {
	uint8_t *address;
	size_t size;
};

const struct bm_retained_clipboard_inst bm_clipboard_inst = {
	.address = (uint8_t *)DT_REG_ADDR(DT_PARENT(DT_INST(0, DT_DRV_COMPAT))),
	.size = DT_REG_SIZE(DT_PARENT(DT_INST(0, DT_DRV_COMPAT)))
};

struct bm_rmem_tlv {
	uint16_t type;
	uint16_t len; /* Data length (not including type and len)*/
} __packed;

#define CRC_TLV_LEN_OFFSET (sizeof(uint32_t) + sizeof(struct bm_rmem_tlv))
#define CRC_TLV_SIZE (CRC_TLV_LEN_OFFSET + sizeof(uint16_t))
#define CRC_TLV_DATA_SIZE (sizeof(uint32_t) + sizeof(uint16_t))

int bm_rmem_writer_init(struct bm_retained_clipboard_ctx *ctx)
{
	ctx->offset = CRC_TLV_SIZE;
	return 0;
}

int bm_rmem_data_write(struct bm_retained_clipboard_ctx *ctx, uint16_t type, const void *data,
		       uint16_t len)
{
	uint8_t *ptr;
	struct bm_rmem_tlv *tlv;

	if (ctx->offset < CRC_TLV_SIZE) {
		return -EINVAL;
	}

	if (ctx->offset + sizeof(struct bm_rmem_tlv) + len > bm_clipboard_inst.size) {
		return -ENOMEM;
	}

	ptr = bm_clipboard_inst.address + ctx->offset;
	tlv = (struct bm_rmem_tlv *)ptr;

	tlv->type = type;
	tlv->len = len;
	memcpy(ptr + sizeof(struct bm_rmem_tlv), data, len);
	ctx->offset += sizeof(struct bm_rmem_tlv) + len;

	return 0;
}

int bm_rmem_crc32_write(struct bm_retained_clipboard_ctx *ctx)
{
	uint16_t len;
	uint32_t checksum;
	uint8_t *ptr;
	struct bm_rmem_tlv *tlv;

	if (ctx->offset < CRC_TLV_SIZE) {
		return -EINVAL;
	}
	/* Calculate the length of the data in clipboard. */
	len = ctx->offset - CRC_TLV_LEN_OFFSET;
	/* Write the length of the data in clipboardto the CRC TLV in advance
	 * to calculate the checksum
	 */
	*(uint16_t *)(bm_clipboard_inst.address + CRC_TLV_LEN_OFFSET) = len;
	checksum = crc32_ieee(bm_clipboard_inst.address + CRC_TLV_LEN_OFFSET, len);

	ptr = bm_clipboard_inst.address;
	tlv = (struct bm_rmem_tlv *)ptr;

	tlv->type = BM_REM_TLV_TYPE_CRC_32;
	tlv->len = CRC_TLV_DATA_SIZE; /* CRC + RMEM dat length covered by the CRC */
	ptr += sizeof(struct bm_rmem_tlv);
	memcpy(ptr, &checksum, sizeof(checksum));
	return 0;
}

int bm_rmem_crc32_verify(void)
{
	struct bm_rmem_tlv *tlv = (struct bm_rmem_tlv *)bm_clipboard_inst.address;

	uint16_t len;
	uint32_t checksum, calculated_checksum;

	if ((tlv->type != BM_REM_TLV_TYPE_CRC_32) || (tlv->len != CRC_TLV_DATA_SIZE)) {
		return -ENOENT;
	}

	len = *(uint16_t *)(bm_clipboard_inst.address + CRC_TLV_LEN_OFFSET);

	if (len > bm_clipboard_inst.size - CRC_TLV_LEN_OFFSET) {
		return -EINVAL;
	}

	checksum = crc32_ieee(bm_clipboard_inst.address + CRC_TLV_LEN_OFFSET, len);
	calculated_checksum = *(uint32_t *)(bm_clipboard_inst.address + sizeof(struct bm_rmem_tlv));

	if (checksum != calculated_checksum) {
		return -EINVAL;
	}
	/* Return the length of the data coverde by the CRC */
	return len;
}

int bm_rmem_reader_init(struct bm_retained_clipboard_ctx *ctx)
{
	int ret = bm_rmem_crc32_verify();

	if (ret < 0) {
		return ret;
	}

	ctx->offset = CRC_TLV_SIZE;
	ctx->max_offset = ret + CRC_TLV_LEN_OFFSET;

	return 0;
}

int bm_rmem_data_get(struct bm_retained_clipboard_ctx *ctx, struct bm_rmem_data_desc *desc)
{
	uint8_t *ptr;
	struct bm_rmem_tlv *tlv;

	if (ctx->offset < CRC_TLV_SIZE) {
		return -EINVAL;
	}

	ptr = bm_clipboard_inst.address + ctx->offset;

	while (ptr < bm_clipboard_inst.address + ctx->max_offset) {
		tlv = (struct bm_rmem_tlv *)ptr;

		if (desc->type == tlv->type) {
			desc->len = tlv->len;
			desc->data = ptr + sizeof(struct bm_rmem_tlv);
			return 0;
		}
		ptr += sizeof(struct bm_rmem_tlv) + tlv->len;
	}

	return -ENOENT;
}

int bm_rmem_clear(void)
{
	memset(bm_clipboard_inst.address, 0, bm_clipboard_inst.size);
	return 0;
}
