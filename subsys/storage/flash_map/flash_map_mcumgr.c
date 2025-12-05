/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

const struct flash_area default_flash_map[] = {
	{
		.fa_id = 1,
		.fa_off = FIXED_PARTITION_OFFSET(slot0_partition),
		.fa_size = FIXED_PARTITION_SIZE(slot0_partition),
		.fa_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller)),
	},
#ifndef CONFIG_MCUBOOT_APPLICATION_FIRMWARE_UPDATER
	{
		.fa_id = 2,
		.fa_off = FIXED_PARTITION_OFFSET(slot1_partition),
		.fa_size = FIXED_PARTITION_SIZE(slot1_partition),
		.fa_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller)),
	},
#endif
};

const int flash_map_entries = ARRAY_SIZE(default_flash_map);
const struct flash_area *flash_map = default_flash_map;
