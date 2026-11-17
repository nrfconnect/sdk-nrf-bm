/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* The in-place update into the update slot.
 *
 * Fixed parameters:
 * - Only plain MCUboot images: an image with an encryption flag set is refused.
 * - No signature or hash check. The image is written as it arrives; the host checks
 *   it against the hash the image list reports.
 */

#include "update.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#define IMAGE_MAGIC		 0x96F3B83DUL
#define IMAGE_F_ENCRYPTED_AES128 0x04UL
#define IMAGE_F_ENCRYPTED_AES256 0x08UL
#define IMAGE_HEADER_LEN	 32U
#define TLV_INFO_MAGIC		 0x6907U
#define TLV_PROT_INFO_MAGIC	 0x6908U
#define TLV_INFO_LEN		 4U
#define TLV_HDR_LEN		 4U
#define TLV_SHA256		 0x10U

/* The upload into the update slot. */
struct session {
	bool is_uploading;
	uint32_t total;
	uint32_t next;
	uint32_t tlv_off;
};

/* The section bounds an MCUboot header gives. */
struct image_layout {
	uint32_t hdr_size;
	uint32_t img_size;
	uint32_t protect_tlv_size;
};

static struct session session;

static uint32_t read_le16(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
	return read_le16(p) | (read_le16(&p[2]) << 16);
}

void update_abort(void)
{
	session.is_uploading = false;
}

uint32_t update_next_offset(void)
{
	return session.next;
}

/* Reads the MCUboot header at @p hdr and checks that a plain image with this layout
 * fits @p total bytes. */
static bool header_read(const uint8_t *hdr, uint32_t total, struct image_layout *layout)
{
	layout->hdr_size = read_le16(&hdr[8]);
	layout->protect_tlv_size = read_le16(&hdr[10]);
	layout->img_size = read_le32(&hdr[12]);

	return (read_le32(&hdr[0]) == IMAGE_MAGIC) &&
	       ((read_le32(&hdr[16]) & (IMAGE_F_ENCRYPTED_AES128 | IMAGE_F_ENCRYPTED_AES256)) ==
		0U) &&
	       (layout->hdr_size >= IMAGE_HEADER_LEN) && (layout->img_size <= total) &&
	       ((total - layout->img_size) >=
		(layout->hdr_size + layout->protect_tlv_size + TLV_INFO_LEN));
}

/* Checks the header at the start of an upload and starts the session. The TLV info
 * the new image will hold is cleared first: until the last chunk writes it, the slot
 * reads as no image, whatever an earlier image left there. */
static enum update_status image_begin(uint32_t total, const uint8_t *data, uint32_t len)
{
	static const uint8_t no_tlv_info[TLV_INFO_LEN];
	struct image_layout layout;

	if ((total > PLATFORM_SLOT_SIZE) || (len < IMAGE_HEADER_LEN) ||
	    !header_read(data, total, &layout)) {
		return UPDATE_REFUSED;
	}

	session.tlv_off = layout.hdr_size + layout.img_size + layout.protect_tlv_size;
	if (!platform_nvm_write(PLATFORM_SLOT_ADDR + session.tlv_off, no_tlv_info,
				sizeof(no_tlv_info))) {
		return UPDATE_WRITE_FAILED;
	}
	session.total = total;
	session.next = 0U;
	session.is_uploading = true;
	return UPDATE_OK;
}

/*
 * Finds the TLV of @p type and @p len in the unprotected TLV area of the image at
 * @p image, which starts at @p tlv_off and ends by @p total. Returns its value, or
 * NULL when the area is malformed or holds no such TLV.
 */
static const uint8_t *tlv_find(const uint8_t *image, uint32_t tlv_off, uint32_t total,
			       uint32_t type, uint32_t len)
{
	if ((total < TLV_INFO_LEN) || (tlv_off > (total - TLV_INFO_LEN)) ||
	    (read_le16(&image[tlv_off]) != TLV_INFO_MAGIC)) {
		return NULL;
	}

	const uint32_t tlv_tot = read_le16(&image[tlv_off + 2U]);

	if ((tlv_tot < TLV_INFO_LEN) || (tlv_tot > (total - tlv_off))) {
		return NULL;
	}

	const uint32_t end = tlv_off + tlv_tot;
	uint32_t pos = tlv_off + TLV_INFO_LEN;

	while ((end - pos) >= TLV_HDR_LEN) {
		const uint32_t tlv_type = read_le16(&image[pos]);
		const uint32_t tlv_len = read_le16(&image[pos + 2U]);

		pos += TLV_HDR_LEN;
		if (tlv_len > (end - pos)) {
			return NULL;
		}
		if ((tlv_type == type) && (tlv_len == len)) {
			return &image[pos];
		}
		pos += tlv_len;
	}
	return NULL;
}

enum update_status update_write(uint32_t off, uint32_t total, const uint8_t *data, uint32_t len)
{
	if (off == 0U) {
		const enum update_status status = image_begin(total, data, len);

		if (status != UPDATE_OK) {
			update_abort();
			return status;
		}
	} else if (!session.is_uploading || (off != session.next)) {
		return UPDATE_OK;
	} else {
		/* The next chunk in order. */
	}

	if ((len > (session.total - session.next)) ||
	    !platform_nvm_write(PLATFORM_SLOT_ADDR + session.next, data, len)) {
		update_abort();
		return UPDATE_WRITE_FAILED;
	}
	session.next += len;

	if (session.next == session.total) {
		const uint8_t *const image = (const uint8_t *)(uintptr_t)PLATFORM_SLOT_ADDR;
		const bool has_tlvs = tlv_find(image, session.tlv_off, session.total, TLV_SHA256,
					       UPDATE_HASH_LEN) != NULL;

		update_abort();
		if (!has_tlvs) {
			return UPDATE_REFUSED;
		}
	}
	return UPDATE_OK;
}

bool update_slot_info_read(struct update_slot_info *info)
{
	const uint8_t *const image = (const uint8_t *)(uintptr_t)PLATFORM_SLOT_ADDR;
	struct image_layout layout;

	/* An upload in progress has not written its TLVs yet. */
	if (session.is_uploading || !header_read(image, PLATFORM_SLOT_SIZE, &layout)) {
		return false;
	}

	uint32_t tlv_off = layout.hdr_size + layout.img_size;

	/* The protected TLV area, when there is one, comes first. */
	if (layout.protect_tlv_size != 0U) {
		if (read_le16(&image[tlv_off]) != TLV_PROT_INFO_MAGIC) {
			return false;
		}
		tlv_off += layout.protect_tlv_size;
	}

	const uint8_t *const hash =
		tlv_find(image, tlv_off, PLATFORM_SLOT_SIZE, TLV_SHA256, UPDATE_HASH_LEN);

	if (hash == NULL) {
		return false;
	}

	info->version_major = image[20];
	info->version_minor = image[21];
	info->version_revision = (uint16_t)read_le16(&image[22]);
	info->version_build = read_le32(&image[24]);
	memcpy(info->hash, hash, UPDATE_HASH_LEN);
	return true;
}
