/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk.h>
#include <version.h>
#include <zephyr_commit.h>
#include <ncs_bare_metal_version.h>
#include <ncs_bare_metal_commit.h>

#if defined(CONFIG_NCS_BARE_METAL_APPLICATION_BOOT_BANNER_STRING)
#include <app_version.h>
#if defined(NCS_APPLICATION_BOOT_BANNER_GIT_REPO)
#include <app_commit.h>
#endif
#endif

#if defined(CONFIG_NCS_BARE_METAL_APPLICATION_BOOT_BANNER_STRING)
#define PREFIX "Using "
#else
#define PREFIX "Booting "
#endif

void boot_banner(void)
{
#if defined(CONFIG_BOOT_DELAY) && (CONFIG_BOOT_DELAY > 0)
	printk("*** Delaying boot by " STRINGIFY(CONFIG_BOOT_DELAY) "ms... ***\n");
	k_busy_wait(CONFIG_BOOT_DELAY * USEC_PER_MSEC);
#endif

#if defined(CONFIG_NCS_BARE_METAL_APPLICATION_BOOT_BANNER_STRING)
	printk("*** Booting " CONFIG_NCS_BARE_METAL_APPLICATION_BOOT_BANNER_STRING " v"
	       APP_VERSION_STRING
#if defined(NCS_APPLICATION_BOOT_BANNER_GIT_REPO)
	       "-" APP_COMMIT_STRING
#else
	       " - unknown commit"
#endif
	       " ***\n");
#endif

#if defined(CONFIG_NCS_BARE_METAL_BOOT_BANNER_STRING)
	printk("*** " PREFIX CONFIG_NCS_BARE_METAL_BOOT_BANNER_STRING " v"
	       NCS_BARE_METAL_VERSION_STRING "-" NCS_BARE_METAL_COMMIT_STRING " ***\n");
#endif

#if defined(CONFIG_NCS_BARE_METAL_ZEPHYR_BOOT_BANNER_STRING)
	printk("*** Using " CONFIG_NCS_BARE_METAL_ZEPHYR_BOOT_BANNER_STRING " v"
	       KERNEL_VERSION_STRING "-" ZEPHYR_COMMIT_STRING " ***\n");
#endif
}
