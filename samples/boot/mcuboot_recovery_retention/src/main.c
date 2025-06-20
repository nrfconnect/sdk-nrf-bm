/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/retention/bootmode.h>
#include <zephyr/kernel.h>
#include <stdio.h>

int main(void)
{
	int rc;

	printf("Waiting...\n");
	k_sleep(K_SECONDS(3));

	rc = bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);

	if (rc == 0) {
		sys_reboot(SYS_REBOOT_WARM);
	} else {
		printf("Error, failed to set boot mode: %d\n", rc);
	}

	return 0;
}
