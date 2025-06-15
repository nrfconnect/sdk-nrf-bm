/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_sdh_soc.h>
#include <zephyr/kernel.h> /* k_busy_wait() */
#include <zephyr/sys_clock.h> /* USEC_PER_SEC */
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/printk.h>
#include <bm_queue.h>

struct item {
	sys_slist_t reserved;
	int foo;
};

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	printk("BLE event %d\n", evt->header.evt_id);
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

static void on_soc_evt(uint32_t evt, void *ctx)
{
	printk("SoC event\n");
}
NRF_SDH_SOC_OBSERVER(sdh_soc, on_soc_evt, NULL, 0);

static void on_state_change(enum nrf_sdh_state_evt state, void *ctx)
{
	printk("SoftDevice state has changed to %d\n", state);
}
NRF_SDH_STATE_EVT_OBSERVER(sdh_state, on_state_change, NULL, 0);

#include <nrf.h>
#include <sys/types.h>

int main(void)
{
	int err;

	printk("Hello SoftDevice sample started\n");

	err = nrf_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		printk("Failed to enable BLE, err %d\n", err);
		return -1;
	}

	printk("Bluetooth enabled\n");

	while (LOG_PROCESS()) {
		/* Empty. */
	}

	k_busy_wait(2 * USEC_PER_SEC);

	err = nrf_sdh_disable_request();
	if (err) {
		printk("Failed to disable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice disabled\n");
	printk("Bye\n");

	while (LOG_PROCESS()) {
		/* Empty. */
	}

	struct k_queue k;
	struct item *element;
	struct item i = {
		.foo = 0xDEADBEEF,
	};
	bool empty;

	k_queue_init(&k);
	element = k_queue_peek_head(&k);
	printk("Head is %p\n", element);
	empty = k_queue_is_empty(&k);
	if (empty) {
		printk("Queue is empty\n");
	} else {
		printk("Queue is not empty\n");
	}

	k_queue_append(&k, &i);
	while (!k_queue_is_empty(&k)) {
		element = k_queue_get(&k, K_NO_WAIT);
		printk("Element is %x\n", element->foo);
	}

#if DONTCOMPILE
	k_queue_prepend(&k, &i);
	k_queue_alloc_append(&k, &i);
	k_queue_alloc_prepend(&k, &i);
	k_queue_append_list(&k, NULL, NULL);
	k_queue_merge_slist(&k, NULL);
	k_queue_get(&k, K_NO_WAIT);
	k_queue_remove(&k, &i);
	k_queue_unique_append(&k, &i);
	k_queue_peek_head(&k);
	k_queue_peek_tail(&k);
	k_queue_is_empty(&k);
#endif

	return 0;
}
