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
	const struct shell *sh = shell_backend_bm_uarte_get_ptr();
	const struct shell_backend_config_flags cfg_flags = SHELL_DEFAULT_BACKEND_CONFIG_FLAGS;
	unsigned int key;

	shell_init(sh, NULL, cfg_flags, false, 0);
	shell_start(sh);
	LOG_INF("shell started");
	while (true) {
		/*
		 * Process all received data from the shell backend, calling registered command
		 * handlers like sample_terminate_cmd if the command and arguments match.
		 */
		shell_process(sh);
		log_flush();

		if (!atomic_get(&running)) {
			/* Terminate requested, end the loop */
			break;
		}

		/*
		 * Ensure no events are missed while we check if there is pending RX data to
		 * be processed.
		 */
		key = irq_lock();

		if (shell_backend_bm_uarte_rx_ready()) {
			/* Process pending RX data */
			irq_unlock(key);
			continue;
		}

		/* Idle until next event */
		k_cpu_atomic_idle(key);
	}

	shell_uninit(sh, NULL);
	LOG_INF("shell terminated");
	while (true) {
		log_flush();
		k_cpu_idle();
	}

	return 0;
}
