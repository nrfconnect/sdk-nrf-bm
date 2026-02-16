/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/sys/util.h>

#include <bm/storage/bm_storage.h>

#include "bm/softdevice_handler/nrf_sdh.h"
#include "cmock_nrf_sdh.h"
#include "cmock_nrf_sdm.h"
#include "cmock_nrf_soc.h"

/* Arbitrary block size. */
#define BLOCK_SIZE 16
#define WORD_SIZE(a) ((a) / sizeof(uint32_t))

/* Arbitrary partition, must be 32-bit word aligned. */
#define PARTITION_START 0x4200
#define PARTITION_SIZE	(BLOCK_SIZE * 3)

#define PTR_IGNORE NULL

/* The backend's SoC event handler */
extern void bm_storage_sd_on_soc_evt(uint32_t evt, void *ctx);
/* The backend's SoftDevice state event handler */
extern int bm_storage_sd_on_state_evt(enum nrf_sdh_state_evt evt, void *ctx);

static struct bm_storage_evt storage_event;

/* Store the two last events, because sometimes one single SoC event generates two events
 * to the application; this way we can test both.
 */
static struct bm_storage_evt storage_events[2];

static bool storage_event_received;
static int storage_event_count;

static void bm_storage_evt_handler(struct bm_storage_evt *evt)
{
	storage_event_received = true;

	storage_event = *evt;
	storage_events[storage_event_count % ARRAY_SIZE(storage_events)] = *evt;

	storage_event_count++;

	switch (evt->id) {
	case BM_STORAGE_EVT_WRITE_RESULT:
		break;
	case BM_STORAGE_EVT_ERASE_RESULT:
		break;
	default:
		break;
	}
}

void test_bm_storage_sd_init_efault(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(NULL, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_init(&storage, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_init(NULL, &config);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_sd_init_eperm(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_sd_init(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_sd_uninit_efault(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_uninit(NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_sd_uninit_eperm(void)
{
	int err;
	struct bm_storage storage = {0};

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_sd_uninit(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_sd_uninit_outstanding(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* Instance has a pending operation, but we don't care */
	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	/* An event is generated regardless */
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(true, storage_event.is_async);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
}

void test_bm_storage_sd_init_uninit_init(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);

	/* Check that the instance can be re-initialized successfully */

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_sd_write_eperm(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_sd_write_einval(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Unaligned length */
	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf) - 1, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	/* Unaligned source */
	err = bm_storage_write(&storage, PARTITION_START, buf + 1, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_sd_write(void)
{
	int err;
	bool is_busy;
	/* Write buffer size must be a multiple of the program unit. */
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* We are busy while writing */
	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_TRUE(is_busy);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(true, storage_event.is_async);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
}

void test_bm_storage_sd_write_retry_etimedout(void)
{
	int err;
	/* Write buffer size must be a multiple of the program unit. */
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	for (int i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_MAX_RETRIES; i++) {
		__cmock_sd_flash_write_ExpectAndReturn((uint32_t *)PARTITION_START, (uint32_t *)buf,
						       WORD_SIZE(sizeof(buf)), 0);

		/* Operation times out and is retried */
		bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_ERROR, NULL);

		/* No event is sent while we are retrying */
		TEST_ASSERT_EQUAL(0, storage_event_received);
	}

	/* The last retry will send an error, and the operation is not retried */
	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_ERROR, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_DISPATCH_MODE_ASYNC, storage_event.dispatch_mode);
	TEST_ASSERT_EQUAL(-ETIMEDOUT, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
}

void test_bm_storage_sd_write_queued(void)
{
	int err;
	bool is_busy;
	uint8_t buf[BLOCK_SIZE];
	uint8_t buf2[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(true, storage_event.is_async);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	/* Before the second operation is started, the SoftDevice changes state.
	 * The backend is ready to change state since no operation is ongoing.
	 */
	is_busy = bm_storage_sd_on_state_evt(NRF_SDH_STATE_EVT_DISABLE_PREPARE, NULL);
	TEST_ASSERT_FALSE(is_busy);

	/* Second call won't trigger a call to the SoftDevice */
	err = bm_storage_write(&storage, PARTITION_START, buf2, sizeof(buf2), NULL);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf2, WORD_SIZE(sizeof(buf2)), 0);

	/* This will trigger the next sd_flash_write() call.
	 * Because the SoftDevice is disabled, the event is sent out immediately.
	 */
	is_busy = bm_storage_sd_on_state_evt(NRF_SDH_STATE_EVT_DISABLED, NULL);
	TEST_ASSERT_FALSE(is_busy);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(false, storage_event.is_async);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf2, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf2), storage_event.len);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_sd_write_retry_queued(void)
{
	int err;
	/* Write buffer size must be a multiple of the program unit. */
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), (void *)0xDEADBEEF);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), (void *)0x0FA7FACE);
	TEST_ASSERT_EQUAL(0, err);

	for (int i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_MAX_RETRIES; i++) {
		__cmock_sd_flash_write_ExpectAndReturn((uint32_t *)PARTITION_START, (uint32_t *)buf,
						       WORD_SIZE(sizeof(buf)), 0);

		/* Operation times out and is retried */
		bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_ERROR, NULL);

		/* No event is sent while we are retrying */
		TEST_ASSERT_EQUAL(0, storage_event_received);
	}

	__cmock_sd_flash_write_ExpectAndReturn((uint32_t *)PARTITION_START, (uint32_t *)buf,
					       WORD_SIZE(sizeof(buf)), 0);

	/* The last retry will send an error, and the operation is not retried,
	 * but the next one is started.
	 */
	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_ERROR, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_DISPATCH_MODE_ASYNC, storage_event.dispatch_mode);
	TEST_ASSERT_EQUAL(-ETIMEDOUT, storage_event.result);
	TEST_ASSERT_EQUAL_PTR(0xDEADBEEF, storage_event.ctx);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_DISPATCH_MODE_ASYNC, storage_event.dispatch_mode);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL_PTR(0x0FA7FACE, storage_event.ctx);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
}

/* Test that when one operation in the queue fails to be scheduled,
 * we continue to process other operations.
 */
void test_bm_storage_sd_write_queued_eio(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* First operation is scheduled immediately and successfully */
	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), (void *)0xDEADBEEF);
	TEST_ASSERT_EQUAL(0, err);

	/* This one fails to be scheduled */
	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), (void *)0xBEEFDEAD);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), (void *)0x0FA7FACE);
	TEST_ASSERT_EQUAL(0, err);

	/* The second operation is not successful, and it's performed after an event is received */
	__cmock_sd_flash_write_ExpectAndReturn((uint32_t *)PARTITION_START, (uint32_t *)buf,
					       WORD_SIZE(sizeof(buf)), NRF_ERROR_INTERNAL);

	/* The queue will jump onto the next operation immediately */
	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	/* First operation has completed, second is rejected and third is scheduled*/
	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	/* First is okay */
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_events[0].id);
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_DISPATCH_MODE_ASYNC, storage_events[0].dispatch_mode);
	TEST_ASSERT_EQUAL_PTR(0xDEADBEEF, storage_events[0].ctx);
	TEST_ASSERT_EQUAL(0, storage_events[0].result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_events[0].addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_events[0].src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_events[0].len);

	/* Second one failed */
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_events[1].id);
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_DISPATCH_MODE_ASYNC, storage_events[1].dispatch_mode);
	TEST_ASSERT_EQUAL_PTR(0xBEEFDEAD, storage_events[1].ctx);
	TEST_ASSERT_EQUAL(-EIO, storage_events[1].result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_events[1].addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_events[1].src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_events[1].len);

	/* Last operation succeeds */
	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_DISPATCH_MODE_ASYNC, storage_event.dispatch_mode);
	TEST_ASSERT_EQUAL_PTR(0x0FA7FACE, storage_event.ctx);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
}

void test_bm_storage_sd_write_enomem(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* If the size of the queue is N, we can queue N+1 elements because the very fist
	 * operation starts immediately, so the space in the queue is freed right away.
	 */
	for (size_t i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE + 1; i++) {
		__cmock_sd_flash_write_ExpectAndReturn(
			(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);
	}

	for (size_t i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE + 1; i++) {
		err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
		TEST_ASSERT_EQUAL(0, err);
	}

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-ENOMEM, err);

	for (size_t i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE + 1; i++) {
		/* Each system events triggers the next operation in the queue */
		bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);
	}
}

void test_bm_storage_sd_write_two_instances(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage storage2 = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), &storage);
	TEST_ASSERT_EQUAL(0, err);

	/* The fist instance has scheduled one operation.
	 * The second instance is initialized.
	 */

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage2, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Upon receiving the SoC event for the fist operation, one event is sent to
	 * the instance that scheduled the operation. The second instance is unaffected.
	 */
	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
	TEST_ASSERT_EQUAL_PTR(&storage, storage_event.ctx);

	/* A second write is requested by the second instance.
	 * The first instance is uninitialized after the new operation is scheduled.
	 */
	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	err = bm_storage_write(&storage2, PARTITION_START, buf, sizeof(buf), &storage2);
	TEST_ASSERT_EQUAL(0, err);

	/* Since `storage` has no pending operations, the unitialization is successful */

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
	TEST_ASSERT_EQUAL_PTR(&storage2, storage_event.ctx);

	TEST_ASSERT_EQUAL(2, storage_event_count);
}

void test_bm_storage_sd_write_disable_prepare(void)
{
	int err;
	bool is_busy;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Before an operation is started, the SoftDevice changes state.
	 * The backend is ready to change state since no operation is ongoing.
	 */
	is_busy = bm_storage_sd_on_state_evt(NRF_SDH_STATE_EVT_DISABLE_PREPARE, NULL);
	TEST_ASSERT_FALSE(is_busy);

	/* This call won't trigger a call to the SoftDevice yet */
	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	/* This will trigger the next sd_flash_write() call.
	 * Because the SoftDevice is disabled, the event is sent out immediately.
	 */
	is_busy = bm_storage_sd_on_state_evt(NRF_SDH_STATE_EVT_DISABLED, NULL);
	TEST_ASSERT_FALSE(is_busy);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(false, storage_event.is_async);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_sd_write_disabled(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	/* SoftDevice is disabled when the storage is initialized */
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){false});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	/* SoC event won't be sent by the SoftDevice */
	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(false, storage_event.is_async);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_sd_write_softdevice_busy_retry(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* SoftDevice is busy with another operation */
	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)),
		NRF_ERROR_BUSY);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* The SoC event will trigger the operation again */
	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	/* The operation completes */
	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(true, storage_event.is_async);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
}

void test_bm_storage_sd_read_eperm(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_read(&storage, PARTITION_START, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_sd_read_einval(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, PARTITION_START, buf, 0);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_sd_read(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	uint32_t dummy_partition[16] = {0x00C0FFEE};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = (uintptr_t)&dummy_partition,
		.end_addr = (uintptr_t)&dummy_partition + sizeof(dummy_partition),
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, config.start_addr, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(0, err);

	TEST_ASSERT_EQUAL_MEMORY(buf, dummy_partition, sizeof(buf));
}

void test_bm_storage_sd_erase_eperm(void)
{
	int err;
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_sd_erase_einval(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE + 1, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_sd_erase(void)
{
	int err;
	bool is_busy;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	for (size_t i = 0; i < BLOCK_SIZE / storage.nvm_info->erase_unit; i++) {
		__cmock_sd_flash_write_ExpectAndReturn(
			(uint32_t *)(PARTITION_START + (i * storage.nvm_info->erase_unit)),
			NULL, WORD_SIZE(storage.nvm_info->erase_unit), 0);
		__cmock_sd_flash_write_IgnoreArg_p_src();
	}

	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(0, err);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_TRUE(is_busy);

	for (size_t i = 0; i < BLOCK_SIZE / storage.nvm_info->erase_unit; i++) {
		bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);
	}

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_ERASE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(NULL, storage_event.src);
	TEST_ASSERT_EQUAL(BLOCK_SIZE, storage_event.len);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);
}

void test_bm_storage_sd_erase_enomem(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* If the size of the queue is N, we can queue N+1 elements because the very fist
	 * operation starts immediately, so the space in the queue is freed right away.
	 */
	for (size_t i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE + 1; i++) {
		__cmock_sd_flash_write_ExpectAndReturn(
			(uint32_t *)PARTITION_START, NULL,
			WORD_SIZE(storage.nvm_info->erase_unit), 0);
		__cmock_sd_flash_write_IgnoreArg_p_src();
	}

	for (size_t i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE + 1; i++) {
		err = bm_storage_erase(&storage, PARTITION_START,
				       storage.nvm_info->erase_unit, NULL);
		TEST_ASSERT_EQUAL(0, err);
	}

	err = bm_storage_erase(&storage, PARTITION_START,
			       storage.nvm_info->erase_unit, NULL);
	TEST_ASSERT_EQUAL(-ENOMEM, err);

	for (size_t i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE + 1; i++) {
		/* Each system events triggers the next operation in the queue */
		bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);
	}
}

void test_bm_storage_sd_is_busy(void)
{
	int err;
	bool is_busy = false;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	is_busy = bm_storage_is_busy(NULL);
	TEST_ASSERT_FALSE(is_busy);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);
}

void test_bm_storage_sd_soc_event_handler(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_sd_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Nothing happens */
	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);
	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_ERROR, NULL);
	/* Non-FLASH event, nothing happens */
	bm_storage_sd_on_soc_evt(NRF_EVT_HFCLKSTARTED, NULL);
	bm_storage_sd_on_soc_evt(NRF_EVT_RADIO_SESSION_IDLE, NULL);
}

void setUp(void)
{
}

void tearDown(void)
{
	memset(&storage_event, 0x00, sizeof(storage_event));
	memset(storage_events, 0x00, sizeof(storage_events));
	storage_event_received = false;
	storage_event_count = 0;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
