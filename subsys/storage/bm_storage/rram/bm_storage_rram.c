/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <mdk/nrf_peripherals.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <nrfx_rramc.h>
#include <bm/storage/bm_storage.h>

/* RRAM word line size in bytes, derived from hardware definition (in bits). */
#define RRAMC_WRITE_BLOCK_SIZE (RRAMC_NRRAMWORDSIZE / BITS_PER_BYTE)

static nrfx_rramc_config_t rramc_config = NRFX_RRAMC_DEFAULT_CONFIG(RRAMC_WRITE_BLOCK_SIZE);

static const struct bm_storage_info bm_storage_info = {
	.program_unit = RRAMC_WRITE_BLOCK_SIZE,
	.erase_unit = RRAMC_WRITE_BLOCK_SIZE,
	.wear_unit = RRAMC_WRITE_BLOCK_SIZE,
	.erase_value = 0xFF,
	.is_erase_before_write = false,
};

struct bm_storage_rram_state {
	uint8_t refcount;
	atomic_t operation_ongoing;
};

static struct bm_storage_rram_state state;

static uint8_t erase_buf[RRAMC_WRITE_BLOCK_SIZE];

static void event_send(const struct bm_storage *storage, struct bm_storage_evt *evt)
{
	if (!storage->evt_handler) {
		return;
	}

	storage->evt_handler(evt);
}

static int bm_storage_rramc_init(struct bm_storage *storage, const struct bm_storage_config *config)
{
	int err;
	unsigned int key;

	err = 0;
	key = irq_lock();

	if (state.refcount == 0) {
		/* First user: initialize hardware */
		err = nrfx_rramc_init(&rramc_config, NULL);
		if (err) {
			goto out;
		}

		state.operation_ongoing = false;

		(void)memset(erase_buf, (int)(bm_storage_info.erase_value & 0xFF),
			     sizeof(erase_buf));
	}

	state.refcount++;
	storage->nvm_info = &bm_storage_info;

out:
	irq_unlock(key);
	return err;
}

static int bm_storage_rramc_uninit(struct bm_storage *storage)
{
	int err;
	unsigned int key;

	err = 0;
	key = irq_lock();

	if (state.refcount > 1) {
		state.refcount--;
	} else {
		/* Last user: uninitialize hardware. */
		if (state.operation_ongoing) {
			err = -EBUSY;
			goto out;
		}

		nrfx_rramc_uninit();

		state.refcount = 0;
	}

out:
	irq_unlock(key);
	return err;
}

static int bm_storage_rramc_read(const struct bm_storage *storage, uint32_t src, void *dest,
				 uint32_t len)
{
	(void)nrfx_rramc_buffer_read(dest, storage->addr + src, len);

	return 0;
}

static int bm_storage_rramc_write(const struct bm_storage *storage, uint32_t dest, const void *src,
				  uint32_t len, void *ctx)
{
	if (!atomic_cas(&state.operation_ongoing, 0, 1)) {
		return -EBUSY;
	}

	nrfx_rramc_bytes_write(storage->addr + dest, src, len);

	atomic_set(&state.operation_ongoing, 0);

	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_WRITE_RESULT,
		.is_async = false,
		.result = 0,
		.addr = dest,
		.src = src,
		.len = len,
		.ctx = ctx
	};

	event_send(storage, &evt);

	return 0;
}

static int bm_storage_rramc_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len,
				  void *ctx)
{
	if (!atomic_cas(&state.operation_ongoing, 0, 1)) {
		return -EBUSY;
	}

	for (uint32_t offset = 0; offset < len; offset += RRAMC_WRITE_BLOCK_SIZE) {
		nrfx_rramc_bytes_write(storage->addr + addr + offset, erase_buf,
				       RRAMC_WRITE_BLOCK_SIZE);
	}

	atomic_set(&state.operation_ongoing, 0);

	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_ERASE_RESULT,
		.is_async = false,
		.result = 0,
		.addr = addr,
		.len = len,
		.ctx = ctx
	};

	event_send(storage, &evt);

	return 0;
}

static bool bm_storage_rramc_is_busy(const struct bm_storage *storage)
{
	return (atomic_get(&state.operation_ongoing) == 1);
}

const struct bm_storage_api bm_storage_rram_api = {
	.init = bm_storage_rramc_init,
	.uninit = bm_storage_rramc_uninit,
	.read = bm_storage_rramc_read,
	.write = bm_storage_rramc_write,
	.erase = bm_storage_rramc_erase,
	.is_busy = bm_storage_rramc_is_busy,
};
