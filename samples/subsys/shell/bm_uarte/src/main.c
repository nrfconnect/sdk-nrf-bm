/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/atomic.h>
#include <bm/shell/backend_bm_uarte.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_SHELL_BM_UARTE_LOG_LEVEL);

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
	LOG_INF("shell started");
	while (atomic_get(&running)) {
		shell_process(shell_backend_bm_uarte_get_ptr());
		log_flush();
		k_busy_wait(10000);
	}
	shell_uninit(shell_backend_bm_uarte_get_ptr(), NULL);
	LOG_INF("shell terminated");
	log_flush();
	return 0;
}
