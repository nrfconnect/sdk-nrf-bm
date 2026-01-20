/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

 #define DT_DRV_COMPAT zephyr_retained_ram

#include <zephyr/logging/log.h>
#include <bm/settings/bluetooth_name.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(settings_bluetooth_name_v2, CONFIG_NCS_BM_FLAT_SETTINGS_LOG_LEVEL);

struct bm_retained_clipboard_inst {
	uint8_t *address;
	size_t size;
};

struct bm_retained_clipboard_ctx {
	uint16_t offset;
	uint16_t max_offset;
};

const struct retained_mem_desc bm_clipboard_inst = {
	.address = (uint8_t *)DT_REG_ADDR(DT_PARENT(DT_INST(0, DT_DRV_COMPAT))),
	.size = DT_REG_SIZE(DT_PARENT(DT_INST(0, DT_DRV_COMPAT)))
};

#define BM_REM_TLV_TYPE_BLE_NAME 0x0001
#define BM_REM_TLV_TYPE_CRC_32 0x0002

struct bm_rmem_tlv {
	uint16_t type;
	uint16 len; /* Data length (not including type and len)*/
} __packed;


bm_rmem_init_writer(struct bm_retained_clipboard_ctx *ctx)
{
	ctx->offset = CRC_TLV_SIZE;
}

static int bm_rmem_write_data(struct bm_retained_clipboard_ctx *ctx, uint16_t type, const void *data, uint16_t len) 
{
	if (ctx->offset < CRC_TLV_SIZE) {
		return -EINVAL;
	}
	if (ctx->offset + sizeof(struct bm_rmem_tlv) + len > bm_clipboard_inst.size) {
		return -ENOMEM;
	}

	uint8_t *ptr = bm_clipboard_inst.address + ctx->offset;
	struct bm_rmem_tlv *tlv = (struct bm_rmem_tlv *)ptr;
	tlv->type = type;
	tlv->len = len;
	memcpy(ptr + sizeof(struct bm_rmem_tlv), data, len);
	ctx->offset += sizeof(struct bm_rmem_tlv) + len;
	return 0;
}


#define CRC_TLV_LEN_OFFSET (sizeof(uint32_t) + sizeof(struct bm_rmem_tlv))
#define CRC_TLV_SIZE (CRC_TLV_LEN_OFFSET + sizeof(uint16_t))

int bm_rmem_wite_crc32(struct bm_retained_clipboard_ctx *ctx) 
{
	if (ctx->offset < CRC_TLV_SIZE) {
		return -EINVAL;
	}
	/* Calculate the length of the data in clipboard. */
	uint16_t len = ctx->offset - CRC_TLV_LEN_OFFSET;
	/* Write the lenght of the data in clipboardto the CRC TLV in advance to calculate the checksum*/
	*(uint16_t *)(bm_clipboard_inst.address + CRC_TLV_LEN_OFFSET) = len;
	uint32_t checksum = crc32_ieee(bm_clipboard_inst.address + CRC_TLV_LEN_OFFSET, len);

	uint8_t *ptr = bm_clipboard_inst.address;
	struct bm_rmem_tlv *tlv = (struct bm_rmem_tlv *)ptr;
	tlv->type = BM_REM_TLV_TYPE_CRC_32;
	tlv->len = CRC_TLV_SIZE;
	ptr += sizeof(struct bm_rmem_tlv);
	memcpy(ptr, &checksum, sizeof(checksum));
	return 0;
}

int bm_rmem_crc32_verify(void)
{
	
	struct bm_rmem_tlv *tlv = (struct bm_rmem_tlv *)bm_clipboard_inst.address;
	if (tlv->type != BM_REM_TLV_TYPE_CRC_32) || (tlv->len != CRC_TLV_SIZE) {
		return -EINVAL;
	}

	uint16_t len = *(uint16_t *)(bm_clipboard_inst.address + CRC_TLV_LEN_OFFSET);
	if (len > bm_clipboard_inst.size - CRC_TLV_LEN_OFFSET) {
		return -EINVAL;
	}

	uint32_t checksum = crc32_ieee(bm_clipboard_inst.address + CRC_TLV_LEN_OFFSET, len);
	uint32_t calculated_checksum = *(uint32_t *)(bm_clipboard_inst.address + sizeof(struct bm_rmem_tlv));
	if (checksum != calculated_checksum) {
		return -EINVAL;
	}
	/* Return the length of the data coverde by the CRC */
	return len;
}

bm_rmem_init_reader(struct bm_retained_clipboard_ctx *ctx)
{
	int ret = bm_rmem_crc32_verify();
	if (ret != 0) {
		return ret;
	}
	ctx->offset = CRC_TLV_SIZE;
	ctx->max_offset = ret + CRC_TLV_LEN_OFFSET;
	return 0;
}

struct bm_rmem_data_desc {
	uint16_t type;
	uint16_t len;
	void *data;
};

int bm_rmem_get_data(struct bm_retained_clipboard_ctx *ctx, struct bm_rmem_data_desc *desc)
{
	if (ctx->offset < CRC_TLV_SIZE) {
		return -EINVAL;
	}

	uint8_t *ptr = bm_clipboard_inst.address + ctx->offset;

	while (ptr < bm_clipboard_inst.address + ctx.max_offset) {
		struct bm_rmem_tlv *tlv = (struct bm_rmem_tlv *)ptr;
		if (desc->type == tlv->type) {
			desc->len = tlv->len;
			desc->data = ptr + sizeof(struct bm_rmem_tlv);
			return 0;
		}
		ptr += sizeof(struct bm_rmem_tlv) + tlv->len;
	}

	return -ENOENT;
}

/* External reference to bluetooth_name_val from bluetooth_name.c */
char bluetooth_name_val[16];

static const char bluetooth_name_key[] = "fw_loader/adv_name";

static int bluetooth_name_value_set(const char *name, const void *value, size_t val_len)
{
	if (name == NULL || value == NULL) {
		return -EINVAL;
	}

	/* Check if the key name matches "fw_loader/adv_name" */
	if (memcmp(name, bluetooth_name_key, sizeof(bluetooth_name_key) - 1) != 0) {
		return -ENOENT;
	}

	/* Check if value length exceeds buffer size (leave room for null terminator) */
	if (val_len > (sizeof(bluetooth_name_val) - 1)) {
		LOG_ERR("Bluetooth name value too long: %zu (max: %zu)", 
			val_len, sizeof(bluetooth_name_val) - 1);
		return -EINVAL;
	}

	/* Copy the value */
	memcpy(bluetooth_name_val, value, val_len);

	/* Ensure null termination */
	bluetooth_name_val[val_len] = '\0';

	LOG_INF("Bluetooth name set to: %s", bluetooth_name_val);

	return 0;
}

int settings_runtime_set(const char *name, const void *data, size_t len)
{
	return bluetooth_name_value_set(name, data, len);
}

int settings_runtime_get(const char *name, void *data, size_t len)
{
	/* TODO: Implement settings_runtime_get */
	(void)name;
	(void)data;
	(void)len;
	return -ENOTSUP;
}

int settings_delete(const char *name)
{
	/* TODO: Implement settings_delete */
	(void)name;
	return -ENOTSUP;
}

int settings_commit(void)
{
	/* TODO: Implement settings_commit */
	return 0;
}

int settings_load(void)
{
	/* TODO: Implement settings_load */
	return 0;
}

int settings_save(void)
{
	/* TODO: Implement settings_save */
	return 0;
}
