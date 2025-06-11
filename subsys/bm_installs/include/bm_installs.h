/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/types.h>

#define BM_INSTALLS_PADDING_SIZE CONFIG_BM_INSTALL_ENTRY_SIZE - \
	((sizeof(struct bm_installs_image) * CONFIG_BM_INSTALL_IMAGES) + sizeof(uint32_t))

enum bm_installs_image_indexes {
	/** SoftDevice image index */
	BM_INSTALLS_IMAGE_INDEX_SOFTDEVICE,

	/** Firmware loader image index */
	BM_INSTALLS_IMAGE_INDEX_FIRMWARE_LOADER,

	/** Total image count */
	BM_INSTALLS_IMAGE_INDEX_COUNT
};

/**
 * @brief Sub-structure for images
 */
struct bm_installs_image {
	/** Start address of image */
	off_t start_address;

	/** Size of image */
	size_t image_size;
};

/**
 * @brief Structure for images
 */
struct bm_installs {
	/** Image array */
	struct bm_installs_image images[CONFIG_BM_INSTALL_IMAGES];

	/** Padding for alignment */
	uint8_t padding[BM_INSTALLS_PADDING_SIZE];

	/** CRC32 checksum of struct (excluding self)  */
	uint32_t checksum;
};

/**
 * @brief Init the bare metal installs subsystem.
 */
void bm_installs_init(void);

/**
 * @brief		Checks if image data is valid. If not valid, then information in images
 *			cannot be retrieved
 *
 * @retval true		On valid.
 * @retval false	On not valid.
 */
bool bm_installs_is_valid(void);

/**
 * @brief		Fetches data for all images.
 *
 * @param data		Pointer to installs struct which will be updated.
 *
 * @retval 0		On success.
 * @retval errno	On error.
 */
int bm_installs_read(struct bm_installs *data);

/**
 * @brief		Fetches data for a single specified image.
 *
 * @param image		Image type to get details on.
 * @param start_address	Pointer to start address which will be updated.
 * @param image_size	Pointer to image size which will be updated.
 *
 * @retval 0		On success.
 * @retval errno	On error.
 */
int bm_installs_get_image_data(uint8_t image, off_t *start_address, size_t *image_size);

/* Functions not for public use */
int bm_installs_write(struct bm_installs *data);
int bm_installs_invalidate(void);
