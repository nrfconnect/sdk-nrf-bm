/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/retention/bootmode.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, CONFIG_SAMPLE_LOG_LEVEL);

int main(void)
{
	int rc;

	LOG_INF("Waiting...");
	k_sleep(K_SECONDS(3));

	rc = bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);

	if (rc == 0) {
		sys_reboot(SYS_REBOOT_WARM);
	} else {
		LOG_INF("Error, failed to set boot mode: %d", rc);
	}

	return 0;
}
