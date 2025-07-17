/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <shell/backend_bm_uarte.h>

#include <zephyr/shell/shell.h>

static atomic_t running = ATOMIC_INIT(1);

static int sample_terminate_cmd(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "goodbye");
	atomic_set(&running, 0);
	return 0;
}

SHELL_CMD_REGISTER(terminate, NULL, "terminate shell", sample_terminate_cmd);

int main(void)
{
	const struct shell_backend_config_flags cfg_flags = SHELL_DEFAULT_BACKEND_CONFIG_FLAGS;

	shell_init(shell_backend_bm_uarte_get_ptr(), NULL, cfg_flags, false, 0);
	shell_start(shell_backend_bm_uarte_get_ptr());
	printk("shell started\n");
	while (atomic_get(&running)) {
		shell_process(shell_backend_bm_uarte_get_ptr());
		k_busy_wait(10000);
	}
	shell_uninit(shell_backend_bm_uarte_get_ptr(), NULL);
	printk("shell terminated\n");
	return 0;
}
