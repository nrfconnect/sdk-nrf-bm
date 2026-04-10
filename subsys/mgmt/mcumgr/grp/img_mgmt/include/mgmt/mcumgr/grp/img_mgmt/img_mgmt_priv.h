/*
 * Copyright (c) 2018-2021 mcumgr authors
 * Copyright (c) 2022-2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_IMG_MGMT_PRIV_
#define H_IMG_MGMT_PRIV_

#include <stdbool.h>
#include <inttypes.h>

#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_MCUBOOT_BOOTLOADER_USES_SHA512
#define IMAGE_TLV_SHA		IMAGE_TLV_SHA512
#define IMAGE_SHA_LEN		64
#else
#define IMAGE_TLV_SHA		IMAGE_TLV_SHA256
#define IMAGE_SHA_LEN		32
#endif

/**
 * @brief Read the specified chunk of data from an image slot.
 *
 * @param slot		The index of the slot to read from.
 * @param offset	The offset within the slot to read from.
 * @param dst		On success, the read data gets written here.
 * @param num_bytes	The number of bytes to read.
 *
 * @return 0 on success, MGMT_ERR_[...] code on failure.
 */
int img_mgmt_read(int slot, unsigned int offset, void *dst, unsigned int num_bytes);

/**
 * @brief Write the specified chunk of image data to slot 1.
 *
 * @param offset	The offset within slot 1 to write to.
 * @param data		The image data to write.
 * @param num_bytes	The number of bytes to write.
 * @param last		Whether this chunk is the end of the image:
 *				false=additional image chunks are forthcoming.
 *				true=last image chunk; flush unwritten data to disk.
 *
 * @return 0 on success, MGMT_ERR_[...] code on failure.
 */
int img_mgmt_write_image_data(unsigned int offset, const void *data, unsigned int num_bytes,
			      bool last);

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
 * @brief Get the slot number of an alternate (inactive) image pair.
 *
 * @param slot	A slot number.
 *
 * @return Number of other slot in pair
 */
static inline int img_mgmt_get_opposite_slot(int slot)
{
	__ASSERT(slot >= 0 && slot < (CONFIG_MCUMGR_GRP_IMG_UPDATABLE_IMAGE_NUMBER << 1),
		 "Impossible slot number");

	return (slot ^ 1);
}

enum img_mgmt_next_boot_type {
	/** The normal boot to active or non-active slot */
	NEXT_BOOT_TYPE_NORMAL	=	0,
	/** The test/non-permanent boot to non-active slot */
	NEXT_BOOT_TYPE_TEST	=	1,
	/** Next boot will be revert to already confirmed slot; this
	 * type of next boot means that active slot is not confirmed
	 * yet as it has been marked for test in previous boot.
	 */
	NEXT_BOOT_TYPE_REVERT	=	2
};

/**
 * @brief Get the next boot slot number for a given image.
 *
 * @param image			An image number.
 * @param type			Type of next boot.
 *
 * @return Number of slot, from pair of slots assigned to image, that will
 * boot on next reset. You need to compare this slot against an active slot
 * to check whether the application image will change for the next boot.
 * @return -1 if the next boot slot cannot be established.
 */
int img_mgmt_get_next_boot_slot(int image, enum img_mgmt_next_boot_type *type);

/**
 * Collect information about the specified image slot.
 *
 * @return Flags of the specified image slot
 */
uint8_t img_mgmt_state_flags(int query_slot);

/**
 * Erase the image data at given offset.
 *
 * @param offset	The offset within slot 1 to erase at.
 * @param num_bytes	The number of bytes to erase.
 *
 * @return 0 on success, MGMT_ERR_[...] code on failure.
 */
int img_mgmt_erase_image_data(unsigned int off, unsigned int num_bytes);

/**
 * Verifies an upload request and indicates the actions to be taken
 * during processing of the request.  This is a "read only" function in the
 * sense that it does not write anything to flash or modify any global
 * variables.
 *
 * @param req		The upload request to inspect.
 * @param action	On success, gets populated with information about how to
 *                      process the request.
 *
 * @return 0 if processing should occur;
 *           A MGMT_ERR code if an error response should be sent instead.
 */
int img_mgmt_upload_inspect(const struct img_mgmt_upload_req *req,
			    struct img_mgmt_upload_action *action);

#define ERASED_VAL_32(x) (((x) << 24) | ((x) << 16) | ((x) << 8) | (x))

int img_mgmt_state_read(struct smp_streamer *ctxt);

#ifdef __cplusplus
}
#endif

#endif
