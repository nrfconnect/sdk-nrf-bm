/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/crc.h>
#include <string.h>
#include <errno.h>
#include <zephyr/devicetree.h>
#include <bm/storage/bm_rmem.h>

#define BM_RMEM_CLIPBOARD_NODE DT_CHOSEN(ncsbm_clipboard_partition)

/**
 * @file bm_rmem.c
 * @brief Implementation of the retention memory clipboard volumen.
 *
 * This file implements simple on SRAM storage aimed to provide inter applications data clipboards.
 * It is used to write and read data to and from the retention memory (in SRAM).
 * Data is written to the retention memory in a TLV (Type-Length-Value) format.
 *
 * Structure of a TLV:
 * +----------------+-----------------------+-----------------+
 * | type (16 bits) | data length (16 bits) | data (variable) |
 * +----------------+-----------------------+-----------------+
 *
 * The clipboard content is preserved using a CRC32 checksum in dedicated CRC32 TLV.
 * The CRC32 TLV is written at the beginning of the retention memory after all the data TLVs are
 * written to verify the integrity of the data.
 *
 * Structure of the CRC32 TLV value field:
 * +-----------------+-------------------------------------------------+
 * | CRC32 (32 bits) | length of the data covered by the CRC (16 bits) |
 * +-----------------+-------------------------------------------------+
 *
 * The length of the data covered by the CRC is the sum of lengths of all the data TLVs and
 * this length field itself.
 *
 * Data TLVs are written one after the other in the retention memory.
 *
 * Structure of the clipboard content:
 * +-----------+----------+----------+-----+----------+
 * | CRC32 TLV | Data TLV | Data TLV | ... | Data TLV |
 * +-----------+----------+----------+-----+----------+
 *
 * How to write data to the clipboard:
 * 1. bm_rmem_init_empty() to initialize the clipboard writer context.
 * 2. bm_rmem_data_write() to write data to the clipboard. Multiple data TLVs can be written.
 * 3. bm_rmem_crc32_write() to write the CRC32 checksum to the clipboard.
 *
 * How to read data from the clipboard:
 * 1. bm_rmem_init() to initialize the clipboard context, verifies the content
 *    integrity.
 * 2. bm_rmem_data_get() to read data from the clipboard. Multiple data TLVs can be read.
 * 3. bm_rmem_clear() optionaly to clear the clipboard content so it will be illegibel for
 *    the next user.

 * After the clipbord is cleared further writes or reads without new initialization are not allowed.
 */

struct bm_retained_clipboard_inst {
	uint8_t *address;
	size_t size;
};

const struct bm_retained_clipboard_inst bm_clipboard_inst = {
	.address = (uint8_t *)DT_REG_ADDR(BM_RMEM_CLIPBOARD_NODE),
	.size = DT_REG_SIZE(BM_RMEM_CLIPBOARD_NODE)
};

struct bm_rmem_tlv {
	uint16_t type;
	uint16_t len; /* Data length (not including type and len). */
} __packed;

#define CRC_TLV_LEN_OFFSET (sizeof(uint32_t) + sizeof(struct bm_rmem_tlv))
#define CRC_TLV_SIZE (CRC_TLV_LEN_OFFSET + sizeof(uint16_t))
#define CRC_TLV_DATA_SIZE (sizeof(uint32_t) + sizeof(uint16_t))

int bm_rmem_init_empty(struct bm_retained_clipboard_ctx *ctx)
{
	ctx->offset = CRC_TLV_SIZE;
	ctx->max_offset = CRC_TLV_SIZE;
	return 0;
}

int bm_rmem_data_write(struct bm_retained_clipboard_ctx *ctx, uint16_t type, const void *data,
		       uint16_t len)
{
	uint8_t *ptr;
	struct bm_rmem_tlv *tlv;

	if (ctx->max_offset < CRC_TLV_SIZE) {
		return -ESPIPE;
	}

	if (ctx->max_offset + sizeof(struct bm_rmem_tlv) + len > bm_clipboard_inst.size) {
		return -ENOMEM;
	}

	ptr = bm_clipboard_inst.address + ctx->max_offset;
	tlv = (struct bm_rmem_tlv *)ptr;

	tlv->type = type;
	tlv->len = len;
	memcpy(ptr + sizeof(struct bm_rmem_tlv), data, len);
	ctx->max_offset += sizeof(struct bm_rmem_tlv) + len;

	return 0;
}

int bm_rmem_crc32_write(struct bm_retained_clipboard_ctx *ctx)
{
	uint16_t len;
	uint32_t checksum;
	uint8_t *ptr;
	struct bm_rmem_tlv *tlv;

	if (ctx->max_offset < CRC_TLV_SIZE) {
		return -EINVAL;
	}

	/* Calculate the length of the data in clipboard. */
	len = ctx->max_offset - CRC_TLV_LEN_OFFSET;

	/* Write the length of the data in clipboard to the CRC TLV in advance
	 * to calculate the checksum.
	 */
	*(uint16_t *)(bm_clipboard_inst.address + CRC_TLV_LEN_OFFSET) = len;
	checksum = crc32_ieee(bm_clipboard_inst.address + CRC_TLV_LEN_OFFSET, len);

	ptr = bm_clipboard_inst.address;
	tlv = (struct bm_rmem_tlv *)ptr;

	tlv->type = BM_RMEM_TLV_TYPE_CRC_32;
	tlv->len = CRC_TLV_DATA_SIZE; /* CRC + RMEM data length covered by the CRC */
	ptr += sizeof(struct bm_rmem_tlv);
	memcpy(ptr, &checksum, sizeof(checksum));
	return 0;
}

int bm_rmem_crc32_verify(void)
{
	struct bm_rmem_tlv *tlv = (struct bm_rmem_tlv *)bm_clipboard_inst.address;

	uint16_t len;
	uint32_t checksum;
	uint32_t calculated_checksum;

	if ((tlv->type != BM_RMEM_TLV_TYPE_CRC_32) || (tlv->len != CRC_TLV_DATA_SIZE)) {
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
	/* Return the length of the data covered by the CRC */
	return len;
}

int bm_rmem_init(struct bm_retained_clipboard_ctx *ctx)
{
	int ret;

	ctx->offset = CRC_TLV_SIZE;
	ctx->max_offset = CRC_TLV_SIZE;

	ret = bm_rmem_crc32_verify();
	if (ret > 0) {
		ctx->max_offset = ret + CRC_TLV_LEN_OFFSET;
	}

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
			if (desc->len > 0) {
				desc->data = ptr + sizeof(struct bm_rmem_tlv);
			} else {
				desc->data = NULL;
			}

			return 0;
		}
		ptr += sizeof(struct bm_rmem_tlv) + tlv->len;
	}

	return -ENOENT;
}

int bm_rmem_clear(struct bm_retained_clipboard_ctx *ctx)
{
	memset(bm_clipboard_inst.address, 0, bm_clipboard_inst.size);
	ctx->offset = CRC_TLV_SIZE;
	ctx->max_offset = CRC_TLV_SIZE;
	return 0;
}
