/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/util.h>

#include <bm/softdevice_handler/nrf_sdh.h>
#include <nrf_soc.h>
#include <hal/nrf_gpio.h>
#include <board-config.h>

#include <bm/storage/bm_storage.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BM_STORAGE_LOG_LEVEL);

#define STORAGE0_PARTITION DT_NODELABEL(storage0_partition)
#define STORAGE0_START DT_REG_ADDR(STORAGE0_PARTITION)
#define STORAGE0_SIZE DT_REG_SIZE(STORAGE0_PARTITION)

/* Write buffer size must be a multiple of the program unit.
 * To support both RRAM (16 bytes) and SoftDevice (4 bytes) backends,
 * that is 16 bytes.
 */
#define BUFFER_BLOCK_SIZE 16

/* Two disjoint storage regions to showcase multiple clients of the storage library. */
#define STORAGE_A_START STORAGE0_START
#define STORAGE_A_END (STORAGE_A_START + BUFFER_BLOCK_SIZE)
#define STORAGE_B_START STORAGE_A_END
#define STORAGE_B_END (STORAGE_B_START + BUFFER_BLOCK_SIZE)

/* Forward declarations. */
static void bm_storage_evt_handler_a(struct bm_storage_evt *evt);
static void bm_storage_evt_handler_b(struct bm_storage_evt *evt);

/* Tracks the number of write operations that are in the process of being executed. */
static volatile int outstanding_writes;

static struct bm_storage storage_a;
static struct bm_storage storage_b;

static void bm_storage_evt_handler_a(struct bm_storage_evt *evt)
{
	switch (evt->id) {
	case BM_STORAGE_EVT_WRITE_RESULT:
		LOG_INF("Handler A: bm_storage_evt: WRITE_RESULT %d, DISPATCH_TYPE %d",
			evt->result, evt->dispatch_type);
		outstanding_writes--;
		break;
	case BM_STORAGE_EVT_ERASE_RESULT:
		/* Not used. */
		break;
	default:
		break;
	}
}

static void bm_storage_evt_handler_b(struct bm_storage_evt *evt)
{
	switch (evt->id) {
	case BM_STORAGE_EVT_WRITE_RESULT:
		LOG_INF("Handler B: bm_storage_evt: WRITE_RESULT %d, DISPATCH_TYPE %d",
			evt->result, evt->dispatch_type);
		outstanding_writes--;
		break;
	case BM_STORAGE_EVT_ERASE_RESULT:
		/* Not used. */
		break;
	default:
		break;
	}
}

static void wait_for_outstanding_writes(void)
{
	LOG_INF("Waiting for writes to complete...");
	while (outstanding_writes > 0) {
		k_cpu_idle();
	}
}

static int storage_inits(void)
{
	int err;

	struct bm_storage_config storage_config_a = {
		.evt_handler = bm_storage_evt_handler_a,
		.start_addr = STORAGE_A_START,
		.end_addr = STORAGE_A_END,
	};

	err = bm_storage_init(&storage_a, &storage_config_a);
	if (err) {
		LOG_ERR("bm_storage_init() failed, err %d", err);
		return err;
	}

	struct bm_storage_config storage_config_b = {
		.evt_handler = bm_storage_evt_handler_b,
		.start_addr = STORAGE_B_START,
		.end_addr = STORAGE_B_END,
	};

	err = bm_storage_init(&storage_b, &storage_config_b);
	if (err) {
		LOG_ERR("bm_storage_init() failed, err %d", err);
		return err;
	}

	return 0;
}

static int storage_uninits(void)
{
	int err;

	err = bm_storage_uninit(&storage_a);
	if (err && err != -ENOTSUP) {
		LOG_ERR("bm_storage_uninit() failed, err %d", err);
		return err;
	}

	err = bm_storage_uninit(&storage_b);
	if (err && err != -ENOTSUP) {
		LOG_ERR("bm_storage_uninit() failed, err %d", err);
		return err;
	}

	return 0;
}

static int storage_writes(void)
{
	int err;
	char input_a[BUFFER_BLOCK_SIZE] = "Hello";
	char input_b[BUFFER_BLOCK_SIZE] = "World!";

	/* Prepare writes. */
	outstanding_writes = 2;

	LOG_INF("Writing in Partition A, addr: 0x%08X, size: %d", storage_a.start_addr,
		sizeof(input_a));

	err = bm_storage_write(&storage_a, storage_a.start_addr, input_a, sizeof(input_a), NULL);
	if (err) {
		LOG_ERR("bm_storage_write() failed, err %d", err);
		return err;
	}

	LOG_INF("Writing in Partition B, addr: 0x%08X, size: %d", storage_b.start_addr,
		sizeof(input_b));

	err = bm_storage_write(&storage_b, storage_b.start_addr, input_b, sizeof(input_b),
			       NULL);
	if (err) {
		LOG_ERR("bm_storage_write() failed, err %d", err);
		return err;
	}

	return 0;
}

static int storage_erases(void)
{
	int err;
	char erase[BUFFER_BLOCK_SIZE] = { 0 };

	/* Prepare writes. */
	outstanding_writes = 2;

	LOG_INF("Erasing in Partition A, addr: 0x%08X, size: %d", storage_a.start_addr,
		sizeof(erase));

	err = bm_storage_write(&storage_a, storage_a.start_addr, erase, sizeof(erase), NULL);
	if (err) {
		LOG_ERR("bm_storage_write() failed, err %d", err);
		return err;
	}

	LOG_INF("Erasing in Partition B, addr: 0x%08X, size: %d", storage_b.start_addr,
		sizeof(erase));

	err = bm_storage_write(&storage_b, storage_b.start_addr, erase, sizeof(erase), NULL);
	if (err) {
		LOG_ERR("bm_storage_write() failed, err %d", err);
		return err;
	}

	return 0;
}

static int storage_reads(void)
{
	int err;
	char output[BUFFER_BLOCK_SIZE] = { 0 };

	err = bm_storage_read(&storage_a, storage_a.start_addr, output, sizeof(output));
	if (err) {
		LOG_ERR("bm_storage_read() failed, err %d", err);
		return err;
	}

	LOG_HEXDUMP_INF(output, sizeof(output), "output A:");

	memset(output, 0, sizeof(output));

	err = bm_storage_read(&storage_b, storage_b.start_addr, output, sizeof(output));
	if (err) {
		LOG_ERR("bm_storage_read() failed, err %d", err);
		return err;
	}

	LOG_HEXDUMP_INF(output, sizeof(output), "output B:");

	return 0;
}

int main(void)
{
	int err;

	LOG_INF("Storage sample started");

	err = nrf_sdh_enable_request();
	if (err) {
		LOG_ERR("Failed to enable SoftDevice, err %d", err);
		goto idle;
	}

	err = storage_inits();
	if (err) {
		goto idle;
	}

	LOG_INF("Reading persisted data");

	err = storage_reads();
	if (err) {
		goto idle;
	}

	err = storage_erases();
	if (err) {
		goto idle;
	}

	wait_for_outstanding_writes();

	err = storage_reads();
	if (err) {
		goto idle;
	}

	err = nrf_sdh_disable_request();
	if (err) {
		LOG_ERR("Failed to disable SoftDevice, err %d", err);
		goto idle;
	}

	err = storage_writes();
	if (err) {
		goto idle;
	}

	wait_for_outstanding_writes();

	err = storage_reads();
	if (err) {
		goto idle;
	}

	err = storage_uninits();
	if (err) {
		goto idle;
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);

	LOG_INF("Storage sample finished.");

idle:
	/* Enter main loop. */
	while (true) {
		log_flush();

		k_cpu_idle();
	}
}
