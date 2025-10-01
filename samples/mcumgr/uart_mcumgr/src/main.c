/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/mgmt/mcumgr/transport/serial.h>
#include <zephyr/drivers/console/uart_mcumgr.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include <nrfx_uarte.h>
#include <smp_uart.h>
#include <board-config.h>

LOG_MODULE_REGISTER(app_uart_mcumgr, CONFIG_APP_UART_MCUMGR_SAMPLE_LOG_LEVEL);

static bool should_reboot;

static enum mgmt_cb_return os_mgmt_reboot_hook(uint32_t event, enum mgmt_cb_return prev_status,
					       int32_t *rc, uint16_t *group, bool *abort_more,
					       void *data, size_t data_size);

static struct mgmt_callback os_mgmt_reboot_callback = {
	.callback = os_mgmt_reboot_hook,
	.event_id = MGMT_EVT_OP_OS_MGMT_RESET,
};

static enum mgmt_cb_return os_mgmt_reboot_hook(uint32_t event, enum mgmt_cb_return prev_status,
					       int32_t *rc, uint16_t *group, bool *abort_more,
					       void *data, size_t data_size)
{
	if (event == MGMT_EVT_OP_OS_MGMT_RESET) {
		should_reboot = true;
		*rc = MGMT_ERR_EOK;

		return MGMT_CB_ERROR_RC;
	}

	return MGMT_CB_OK;
}

int main(void)
{
	LOG_INF("UART MCUmgr sample started");

	mgmt_callback_register(&os_mgmt_reboot_callback);

	while (should_reboot == false) {
		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();

		smp_uart_process_rx_queue();
	}

	sys_reboot(SYS_REBOOT_WARM);
}
