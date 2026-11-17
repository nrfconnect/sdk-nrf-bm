/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* The SMP server for the in-place update.
 *
 * Fixed parameters:
 * - Commands: MCUmgr parameters (0/6), bootloader information (0/8), reset (0/5),
 *   image state read (1/0) and image upload (1/1). Everything else is "not
 *   supported".
 * - Upload "image" 1, the update slot in place, only. The first chunk holds the whole
 *   MCUboot header.
 * - The image state lists the update slot as image 1, slot 0, when it holds a
 *   complete image: never active, confirmed or pending.
 * - Responses carry "rc", the SMP version 1 form. No image test, confirm or erase.
 */

#include "smp.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "update.h"

#define SMP_HDR_LEN 8U
#define SMP_OP_MASK 0x07U

#define SMP_OP_READ  0U
#define SMP_OP_WRITE 2U

#define SMP_GROUP_OS	0U
#define SMP_GROUP_IMAGE 1U

#define SMP_ID_RESET	       5U
#define SMP_ID_MCUMGR_PARAMS   6U
#define SMP_ID_BOOTLOADER_INFO 8U
#define SMP_ID_IMAGE_STATE     0U
#define SMP_ID_UPLOAD	       1U

#define MGMT_ERR_EOK	  0U
#define MGMT_ERR_EUNKNOWN 1U
#define MGMT_ERR_EINVAL	  3U
#define MGMT_ERR_ENOTSUP  8U

/* The upload "image" that writes the update slot in place, and the slot the image
 * state reports it as. */
#define UPLOAD_IMAGE 1U
#define UPLOAD_SLOT  0U

/* The SMP requests the transport takes at a time. */
#define SMP_REQ_COUNT 1U

/* enum mcuboot_mode in MCUboot's bootutil/boot_status.h. */
#define MCUBOOT_MODE_FIRMWARE_LOADER 7U

#define CBOR_UINT   0U
#define CBOR_BSTR   2U
#define CBOR_TSTR   3U
#define CBOR_ARRAY  4U
#define CBOR_MAP    5U
#define CBOR_SIMPLE 7U
#define CBOR_FALSE  0xF4U
#define CBOR_TRUE   0xF5U

/* "255.255.65535.4294967295" */
#define VERSION_TEXT_MAX 24U

/* The fields of an SMP request this server reads. Absent unsigned fields are 0. */
struct smp_fields {
	uint32_t image;
	uint32_t off;
	uint32_t len;
	const uint8_t *data;
	uint32_t data_len;
	bool has_off;
	bool has_len;
	bool is_mode_query;
};

struct cbor_reader {
	const uint8_t *p;
	const uint8_t *end;
};

struct cbor_writer {
	uint8_t *p;
	const uint8_t *end;
};

static const char cbor_key_image[] = "image";
static const char cbor_key_off[] = "off";
static const char cbor_key_len[] = "len";
static const char cbor_key_data[] = "data";
static const char cbor_key_query[] = "query";
static const char cbor_key_rc[] = "rc";
static const char cbor_key_images[] = "images";
static const char cbor_key_slot[] = "slot";
static const char cbor_key_version[] = "version";
static const char cbor_key_hash[] = "hash";
static const char cbor_key_bootable[] = "bootable";
static const char cbor_key_pending[] = "pending";
static const char cbor_key_confirmed[] = "confirmed";
static const char cbor_key_active[] = "active";
static const char cbor_key_permanent[] = "permanent";
static const char cbor_key_buf_size[] = "buf_size";
static const char cbor_key_buf_count[] = "buf_count";
static const char cbor_key_bootloader[] = "bootloader";
static const char cbor_key_mode[] = "mode";
static const char cbor_key_no_downgrade[] = "no-downgrade";
static const char bootloader_name[] = "IronSide";

#define TEXT_LEN(text) ((uint32_t)sizeof(text) - 1U)

static uint32_t read_be16(const uint8_t *p)
{
	return ((uint32_t)p[0] << 8) | (uint32_t)p[1];
}

/* Integer arguments wider than 32 bits and indefinite lengths are refused. */
static bool cbor_head_read(struct cbor_reader *c, uint32_t *major, uint32_t *arg)
{
	if (c->p >= c->end) {
		return false;
	}

	const uint32_t initial = *c->p++;
	const uint32_t info = initial & 0x1FU;

	*major = initial >> 5;
	if (info < 24U) {
		*arg = info;
		return true;
	}
	if (info > 26U) {
		return false;
	}

	const uint32_t n = 1U << (info - 24U);

	if ((uint32_t)(c->end - c->p) < n) {
		return false;
	}
	*arg = 0U;
	for (uint32_t i = 0U; i < n; i++) {
		*arg = (*arg << 8) | *c->p++;
	}
	return true;
}

static bool cbor_string_read(struct cbor_reader *c, uint32_t major, const uint8_t **str,
			     uint32_t *len)
{
	uint32_t m = 0U;

	if (!cbor_head_read(c, &m, len) || (m != major) || ((uint32_t)(c->end - c->p) < *len)) {
		return false;
	}
	*str = c->p;
	c->p += *len;
	return true;
}

static bool cbor_head_write(struct cbor_writer *c, uint32_t major, uint32_t arg)
{
	uint32_t n = 4U;
	uint32_t info = 26U;

	if (arg < 24U) {
		n = 0U;
		info = arg;
	} else if (arg <= 0xFFU) {
		n = 1U;
		info = 24U;
	} else if (arg <= 0xFFFFU) {
		n = 2U;
		info = 25U;
	} else {
		/* Four argument bytes. */
	}

	if ((uint32_t)(c->end - c->p) < (1U + n)) {
		return false;
	}
	*c->p++ = (uint8_t)((major << 5) | info);
	for (uint32_t i = n; i > 0U; i--) {
		*c->p++ = (uint8_t)(arg >> (8U * (i - 1U)));
	}
	return true;
}

static bool cbor_text_write(struct cbor_writer *c, const char *text, uint32_t len)
{
	if (!cbor_head_write(c, CBOR_TSTR, len) || ((uint32_t)(c->end - c->p) < len)) {
		return false;
	}
	memcpy(c->p, text, len);
	c->p += len;
	return true;
}

static bool cbor_bytes_write(struct cbor_writer *c, const uint8_t *bytes, uint32_t len)
{
	if (!cbor_head_write(c, CBOR_BSTR, len) || ((uint32_t)(c->end - c->p) < len)) {
		return false;
	}
	memcpy(c->p, bytes, len);
	c->p += len;
	return true;
}

static bool cbor_bool_write(struct cbor_writer *c, bool value)
{
	if (c->p >= c->end) {
		return false;
	}
	*c->p++ = value ? CBOR_TRUE : CBOR_FALSE;
	return true;
}

static bool cbor_key_uint_write(struct cbor_writer *c, const char *key, uint32_t key_len,
				uint32_t value)
{
	return cbor_text_write(c, key, key_len) && cbor_head_write(c, CBOR_UINT, value);
}

static bool cbor_key_bool_write(struct cbor_writer *c, const char *key, uint32_t key_len,
				bool value)
{
	return cbor_text_write(c, key, key_len) && cbor_bool_write(c, value);
}

static bool is_key(const uint8_t *key, uint32_t key_len, const char *name, uint32_t name_len)
{
	return (key_len == name_len) && (memcmp(key, name, name_len) == 0);
}

/* Reads the request map. An empty payload reads as an empty map, as hosts send read
 * requests without one. Values under unknown keys are skipped when they are
 * integers, strings or simple values; nested items are refused. */
static bool smp_fields_read(struct cbor_reader *c, struct smp_fields *f)
{
	uint32_t major = 0U;
	uint32_t pairs = 0U;

	memset(f, 0, sizeof(*f));
	if (c->p == c->end) {
		return true;
	}
	if (!cbor_head_read(c, &major, &pairs) || (major != CBOR_MAP)) {
		return false;
	}

	for (uint32_t i = 0U; i < pairs; i++) {
		const uint8_t *key = NULL;
		const uint8_t *str = NULL;
		uint32_t key_len = 0U;
		uint32_t value = 0U;

		if (!cbor_string_read(c, CBOR_TSTR, &key, &key_len)) {
			return false;
		}

		const uint8_t *const value_start = c->p;

		if (!cbor_head_read(c, &major, &value)) {
			return false;
		}
		if ((major == CBOR_BSTR) || (major == CBOR_TSTR)) {
			c->p = value_start;
			if (!cbor_string_read(c, major, &str, &value)) {
				return false;
			}
		}

		if ((major == CBOR_UINT) &&
		    is_key(key, key_len, cbor_key_image, TEXT_LEN(cbor_key_image))) {
			f->image = value;
		} else if ((major == CBOR_UINT) &&
			   is_key(key, key_len, cbor_key_off, TEXT_LEN(cbor_key_off))) {
			f->off = value;
			f->has_off = true;
		} else if ((major == CBOR_UINT) &&
			   is_key(key, key_len, cbor_key_len, TEXT_LEN(cbor_key_len))) {
			f->len = value;
			f->has_len = true;
		} else if ((major == CBOR_BSTR) &&
			   is_key(key, key_len, cbor_key_data, TEXT_LEN(cbor_key_data))) {
			f->data = str;
			f->data_len = value;
		} else if ((major == CBOR_TSTR) &&
			   is_key(key, key_len, cbor_key_query, TEXT_LEN(cbor_key_query))) {
			f->is_mode_query =
				is_key(str, value, cbor_key_mode, TEXT_LEN(cbor_key_mode));
		} else if ((major > CBOR_TSTR) && (major != CBOR_SIMPLE)) {
			return false;
		} else {
			/* A value under a key this server ignores, such as "sha" or "upgrade". */
		}
	}
	return true;
}

static bool rc_write(struct cbor_writer *w, uint32_t rc)
{
	return cbor_head_write(w, CBOR_MAP, 1U) &&
	       cbor_text_write(w, cbor_key_rc, TEXT_LEN(cbor_key_rc)) &&
	       cbor_head_write(w, CBOR_UINT, rc);
}

static uint32_t upload_chunk_write(const struct smp_fields *f)
{
	if (!f->has_off || (f->data == NULL) || ((f->off == 0U) && !f->has_len)) {
		return MGMT_ERR_EINVAL;
	}
	if ((f->off == 0U) && (f->image != UPLOAD_IMAGE)) {
		return MGMT_ERR_ENOTSUP;
	}

	switch (update_write(f->off, f->len, f->data, f->data_len)) {
	case UPDATE_OK:
		return MGMT_ERR_EOK;
	case UPDATE_REFUSED:
		return MGMT_ERR_EINVAL;
	case UPDATE_WRITE_FAILED:
	default:
		return MGMT_ERR_EUNKNOWN;
	}
}

static bool upload_serve(const struct smp_fields *f, struct cbor_writer *w)
{
	const uint32_t rc = upload_chunk_write(f);

	return cbor_head_write(w, CBOR_MAP, 2U) &&
	       cbor_text_write(w, cbor_key_rc, TEXT_LEN(cbor_key_rc)) &&
	       cbor_head_write(w, CBOR_UINT, rc) &&
	       cbor_text_write(w, cbor_key_off, TEXT_LEN(cbor_key_off)) &&
	       cbor_head_write(w, CBOR_UINT, update_next_offset());
}

/* Writes "major.minor.revision", and ".build" when the build number is not 0, as
 * MCUmgr's image management does. */
static uint32_t version_text_write(const struct update_slot_info *info, char text[VERSION_TEXT_MAX])
{
	const uint32_t parts[] = {info->version_major, info->version_minor, info->version_revision,
				  info->version_build};
	const uint32_t part_count = (info->version_build != 0U) ? 4U : 3U;
	uint32_t len = 0U;

	for (uint32_t i = 0U; i < part_count; i++) {
		char digits[10];
		uint32_t digit_count = 0U;
		uint32_t value = parts[i];

		if (i != 0U) {
			text[len++] = '.';
		}
		do {
			digits[digit_count++] = (char)('0' + (value % 10U));
			value /= 10U;
		} while (value != 0U);
		while (digit_count > 0U) {
			text[len++] = digits[--digit_count];
		}
	}
	return len;
}

static bool image_state_serve(struct cbor_writer *w)
{
	struct update_slot_info info;
	char version[VERSION_TEXT_MAX];

	if (!update_slot_info_read(&info)) {
		return cbor_head_write(w, CBOR_MAP, 1U) &&
		       cbor_text_write(w, cbor_key_images, TEXT_LEN(cbor_key_images)) &&
		       cbor_head_write(w, CBOR_ARRAY, 0U);
	}

	const uint32_t version_len = version_text_write(&info, version);

	return cbor_head_write(w, CBOR_MAP, 1U) &&
	       cbor_text_write(w, cbor_key_images, TEXT_LEN(cbor_key_images)) &&
	       cbor_head_write(w, CBOR_ARRAY, 1U) && cbor_head_write(w, CBOR_MAP, 9U) &&
	       cbor_key_uint_write(w, cbor_key_image, TEXT_LEN(cbor_key_image), UPLOAD_IMAGE) &&
	       cbor_key_uint_write(w, cbor_key_slot, TEXT_LEN(cbor_key_slot), UPLOAD_SLOT) &&
	       cbor_text_write(w, cbor_key_version, TEXT_LEN(cbor_key_version)) &&
	       cbor_text_write(w, version, version_len) &&
	       cbor_text_write(w, cbor_key_hash, TEXT_LEN(cbor_key_hash)) &&
	       cbor_bytes_write(w, info.hash, UPDATE_HASH_LEN) &&
	       cbor_key_bool_write(w, cbor_key_bootable, TEXT_LEN(cbor_key_bootable), true) &&
	       cbor_key_bool_write(w, cbor_key_pending, TEXT_LEN(cbor_key_pending), false) &&
	       cbor_key_bool_write(w, cbor_key_confirmed, TEXT_LEN(cbor_key_confirmed), false) &&
	       cbor_key_bool_write(w, cbor_key_active, TEXT_LEN(cbor_key_active), false) &&
	       cbor_key_bool_write(w, cbor_key_permanent, TEXT_LEN(cbor_key_permanent), false);
}

static bool mcumgr_params_serve(struct cbor_writer *w)
{
	return cbor_head_write(w, CBOR_MAP, 2U) &&
	       cbor_key_uint_write(w, cbor_key_buf_size, TEXT_LEN(cbor_key_buf_size),
				   SMP_REQ_MAX) &&
	       cbor_key_uint_write(w, cbor_key_buf_count, TEXT_LEN(cbor_key_buf_count),
				   SMP_REQ_COUNT);
}

static bool bootloader_info_serve(const struct smp_fields *f, struct cbor_writer *w)
{
	if (f->is_mode_query) {
		return cbor_head_write(w, CBOR_MAP, 2U) &&
		       cbor_text_write(w, cbor_key_mode, TEXT_LEN(cbor_key_mode)) &&
		       cbor_head_write(w, CBOR_UINT, MCUBOOT_MODE_FIRMWARE_LOADER) &&
		       cbor_key_bool_write(w, cbor_key_no_downgrade,
					   TEXT_LEN(cbor_key_no_downgrade), true);
	}
	return cbor_head_write(w, CBOR_MAP, 1U) &&
	       cbor_text_write(w, cbor_key_bootloader, TEXT_LEN(cbor_key_bootloader)) &&
	       cbor_text_write(w, bootloader_name, TEXT_LEN(bootloader_name));
}

static bool smp_command_serve(uint32_t op, uint32_t group, uint32_t id, struct cbor_reader *req,
			      struct cbor_writer *w, bool *is_reset)
{
	struct smp_fields f;

	if (!smp_fields_read(req, &f)) {
		return rc_write(w, MGMT_ERR_EINVAL);
	}
	if ((op == SMP_OP_WRITE) && (group == SMP_GROUP_IMAGE) && (id == SMP_ID_UPLOAD)) {
		return upload_serve(&f, w);
	}
	if ((op == SMP_OP_READ) && (group == SMP_GROUP_IMAGE) && (id == SMP_ID_IMAGE_STATE)) {
		return image_state_serve(w);
	}
	if ((op == SMP_OP_READ) && (group == SMP_GROUP_OS) && (id == SMP_ID_MCUMGR_PARAMS)) {
		return mcumgr_params_serve(w);
	}
	if ((op == SMP_OP_READ) && (group == SMP_GROUP_OS) && (id == SMP_ID_BOOTLOADER_INFO)) {
		return bootloader_info_serve(&f, w);
	}
	if ((op == SMP_OP_WRITE) && (group == SMP_GROUP_OS) && (id == SMP_ID_RESET)) {
		*is_reset = true;
		return rc_write(w, MGMT_ERR_EOK);
	}
	return rc_write(w, MGMT_ERR_ENOTSUP);
}

uint32_t smp_packet_serve(const uint8_t *pkt, uint32_t len, uint8_t *rsp, uint32_t rsp_size,
			  bool *is_reset)
{
	*is_reset = false;
	if ((len < SMP_HDR_LEN) || (rsp_size < SMP_HDR_LEN) ||
	    (read_be16(&pkt[2]) != (len - SMP_HDR_LEN))) {
		return 0U;
	}

	const uint32_t op = pkt[0] & SMP_OP_MASK;
	struct cbor_reader req = {.p = &pkt[SMP_HDR_LEN], .end = &pkt[len]};
	struct cbor_writer w = {.p = &rsp[SMP_HDR_LEN], .end = &rsp[rsp_size]};

	if (!smp_command_serve(op, read_be16(&pkt[4]), pkt[7], &req, &w, is_reset)) {
		*is_reset = false;
		return 0U;
	}

	const uint32_t rsp_len = (uint32_t)(w.p - rsp);
	const uint32_t body_len = rsp_len - SMP_HDR_LEN;

	/* Read (0) and write (2) are answered with the next operation code. */
	memcpy(rsp, pkt, SMP_HDR_LEN);
	rsp[0] = (uint8_t)((pkt[0] & ~SMP_OP_MASK) | (op + 1U));
	rsp[2] = (uint8_t)(body_len >> 8);
	rsp[3] = (uint8_t)body_len;
	return rsp_len;
}
