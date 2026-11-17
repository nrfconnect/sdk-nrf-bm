/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* The in-place update: a plain MCUboot image written into the update slot as it
 * arrives.
 */

#ifndef BLE_SMP_UPDATE_H_
#define BLE_SMP_UPDATE_H_

#include <stdbool.h>
#include <stdint.h>

/* The length of the image's SHA-256 hash TLV. */
#define UPDATE_HASH_LEN 32U

enum update_status {
	UPDATE_OK,
	/* The image does not fit the update slot, or is not a plain MCUboot image. */
	UPDATE_REFUSED,
	UPDATE_WRITE_FAILED,
};

/* The MCUboot image the update slot holds. */
struct update_slot_info {
	uint8_t version_major;
	uint8_t version_minor;
	uint16_t version_revision;
	uint32_t version_build;
	uint8_t hash[UPDATE_HASH_LEN];
};

/*
 * Writes @p len bytes of the image at offset @p off. Offset 0 starts the image,
 * @p total bytes long, and @p data then holds its whole header. A chunk at another
 * offset than update_next_offset() is ignored with UPDATE_OK, so the host resumes
 * there. The chunk that completes the image ends the session.
 */
enum update_status update_write(uint32_t off, uint32_t total, const uint8_t *data, uint32_t len);

uint32_t update_next_offset(void);

/* Ends the session. */
void update_abort(void);

/* Reads the version and hash of the image in the update slot. Returns false when
 * the slot holds no complete MCUboot image with a SHA-256 hash TLV. */
bool update_slot_info_read(struct update_slot_info *info);

#endif /* BLE_SMP_UPDATE_H_ */
