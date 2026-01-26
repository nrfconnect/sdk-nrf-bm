/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <bm/storage/bm_storage.h>
#include <bm/storage/bm_storage_backend.h>

#include "bm/softdevice_handler/nrf_sdh.h"
#include "cmock_nrf_sdh.h"
#include "cmock_nrf_sdm.h"
#include "cmock_nrf_soc.h"

/* This is used to size buffers.
 * A buffer is as large as a chunk (so it is written in one operation).
 */
#define BLOCK_SIZE CONFIG_BM_STORAGE_BACKEND_SD_MAX_WRITE_SIZE

/* Arbitrary partition, must be 32-bit word aligned. */
#define PARTITION_START 0x4200
#define PARTITION_SIZE	(BLOCK_SIZE * 3)

#define WORD_SIZE(a) ((a) / sizeof(uint32_t))
#define PTR_IGNORE NULL

/* The backend's SoC event handler */
extern void bm_storage_sd_on_soc_evt(uint32_t evt, void *ctx);
/* The backend's SoftDevice state event handler */
extern int bm_storage_sd_on_state_evt(enum nrf_sdh_state_evt evt, void *ctx);

static struct bm_storage_evt storage_event;
static bool storage_event_received;

static void bm_storage_evt_handler(struct bm_storage_evt *evt)
{
	storage_event_received = true;
	memcpy(&storage_event, evt, sizeof(*evt));

	switch (evt->id) {
	case BM_STORAGE_EVT_WRITE_RESULT:
		break;
	case BM_STORAGE_EVT_ERASE_RESULT:
		break;
	default:
		break;
	}
}

void test_bm_storage_init_efault(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

void test_bm_storage_init_eperm(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

void test_bm_storage_init(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_uninit_efault(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

void test_bm_storage_uninit_eperm(void)
{
	int err;
	struct bm_storage storage = {0};

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_uninit(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

void test_bm_storage_uninit_outstanding(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
}

void test_bm_storage_init_uninit_init(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

void test_bm_storage_write_eperm(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_write_einval(void)
{
	int err;
	/* Write buffer size must be a multiple of the program unit.
	 * This will cause an error.
	 */
	uint8_t buf[BLOCK_SIZE - 1];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_write_efault(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(&storage, PARTITION_START, buf + 1, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_write(void)
{
	int err;
	bool is_busy;
	/* Write buffer size must be a multiple of the program unit. */
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
}

void test_bm_storage_write_queued(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	uint8_t buf2[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

	/* Second call won't trigger a call to the SoftDevice */
	err = bm_storage_write(&storage, PARTITION_START, buf2, sizeof(buf2), NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* The SoC event from the first operation will trigger the second */
	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf2, WORD_SIZE(sizeof(buf2)), 0);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf2, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf2), storage_event.len);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_write_eio(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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
	TEST_ASSERT_EQUAL(-EIO, err);

	for (size_t i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE + 1; i++) {
		/* Each system events triggers the next operation in the queue */
		bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);
	}
}

void test_bm_storage_write_queued_disable_prepare_busy(void)
{
	int err;
	bool is_busy;
	uint8_t buf[BLOCK_SIZE];
	uint8_t buf2[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

	/* Second call won't trigger a call to the SoftDevice */
	err = bm_storage_write(&storage, PARTITION_START, buf2, sizeof(buf2), NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* Before the second operation is started, the SoftDevice changes state */
	is_busy = bm_storage_sd_on_state_evt(NRF_SDH_STATE_EVT_DISABLE_PREPARE, NULL);
	TEST_ASSERT_TRUE(is_busy);

	/* The operation completes, but the next one is not started.
	 * Instead, we notify that we are ready.
	 */
	__cmock_nrf_sdh_observer_ready_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_nrf_sdh_observer_ready_IgnoreArg_observer();

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf2, WORD_SIZE(sizeof(buf2)), 0);

	/* This will trigger the next sd_flash_write() call.
	 * Because the SoftDevice is disabled, the event is sent out immediately.
	 */
	is_busy = bm_storage_sd_on_state_evt(NRF_SDH_STATE_EVT_DISABLED, NULL);
	TEST_ASSERT_FALSE(is_busy);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf2, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf2), storage_event.len);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_write_queued_disable_prepare_nonbusy(void)
{
	int err;
	bool is_busy;
	uint8_t buf[BLOCK_SIZE];
	uint8_t buf2[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf2, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf2), storage_event.len);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_write_disable_prepare(void)
{
	int err;
	bool is_busy;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Before an second operation is started, the SoftDevice changes state.
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
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_write_disabled(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_write_softdevice_busy_retry(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
}

void test_bm_storage_write_chunk(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE * 2];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* First chunk, up to BLOCK_SIZE */
	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(BLOCK_SIZE), 0);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* The SoC event from the first operation will trigger the second */
	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)(PARTITION_START + BLOCK_SIZE),
		(uint32_t *)(buf + BLOCK_SIZE),
		WORD_SIZE(sizeof(buf) - BLOCK_SIZE), 0);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_FALSE(storage_event_received);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_read_eperm(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_read(&storage, PARTITION_START, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_read_einval(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

void test_bm_storage_read(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	uint32_t dummy_partition[16] = {0x00C0FFEE};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

void test_bm_storage_erase_eperm(void)
{
	int err;
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_erase_einval(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

void test_bm_storage_erase(void)
{
	int err;
	bool is_busy;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, PTR_IGNORE, WORD_SIZE(BLOCK_SIZE), 0);
	__cmock_sd_flash_write_IgnoreArg_p_src();

	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(0, err);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_TRUE(is_busy);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_ERASE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(NULL, storage_event.src);
	TEST_ASSERT_EQUAL(BLOCK_SIZE, storage_event.len);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);
}

void test_bm_storage_erase_chunk(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	const uint32_t big_block = BLOCK_SIZE + storage.nvm_info->erase_unit;

	/* First chunk, up to _MAX_WRITE_SIZE */
	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, PTR_IGNORE, WORD_SIZE(BLOCK_SIZE), 0);
	__cmock_sd_flash_write_IgnoreArg_p_src();

	err = bm_storage_erase(&storage, PARTITION_START, big_block, NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* The SoC event from the first operation will trigger the second */
	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)(PARTITION_START + BLOCK_SIZE), PTR_IGNORE,
		WORD_SIZE(storage.nvm_info->erase_unit), 0);
	__cmock_sd_flash_write_IgnoreArg_p_src();

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_FALSE(storage_event_received);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_ERASE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(NULL, storage_event.src);
	TEST_ASSERT_EQUAL(big_block, storage_event.len);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_erase_odd(void)
{
	int err;
	bool is_busy;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(PTR_IGNORE, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){true});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* A multiple of the erase unit, but smaller than _MAX_WRITE_SIZE */
	const uint32_t small_block = storage.nvm_info->erase_unit * 2;

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, PTR_IGNORE, WORD_SIZE(small_block), 0);
	__cmock_sd_flash_write_IgnoreArg_p_src();

	/* Write a multiple of the erase unit (still smaller than _MAX_WRITE_SIZE )*/
	err = bm_storage_erase(&storage, PARTITION_START, small_block, NULL);
	TEST_ASSERT_EQUAL(0, err);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_TRUE(is_busy);

	bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_ERASE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(NULL, storage_event.src);
	TEST_ASSERT_EQUAL(small_block, storage_event.len);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);
}

void test_bm_storage_erase_eio(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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
			(uint32_t *)PARTITION_START, PTR_IGNORE,
			WORD_SIZE(storage.nvm_info->erase_unit), 0);
		__cmock_sd_flash_write_IgnoreArg_p_src();
	}

	for (size_t i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE + 1; i++) {
		err = bm_storage_erase(&storage, PARTITION_START,
				       sizeof(storage.nvm_info->erase_unit), NULL);
		TEST_ASSERT_EQUAL(0, err);
	}

	err = bm_storage_erase(&storage, PARTITION_START,
			       sizeof(storage.nvm_info->erase_unit), NULL);
	TEST_ASSERT_EQUAL(-EIO, err);

	for (size_t i = 0; i < CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE + 1; i++) {
		/* Each system events triggers the next operation in the queue */
		bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);
	}
}

void test_bm_storage_is_busy(void)
{
	int err;
	bool is_busy = false;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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

void test_bm_storage_soc_event_handler(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
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
	storage_event_received = false;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
