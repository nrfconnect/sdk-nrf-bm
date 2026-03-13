/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <stdint.h>
#include <zephyr/sys/util.h>
#include <bm/storage/bm_storage.h>

#define NATIVE_SIM_PAD_UNIT 16

static uint8_t pad_buffer[NATIVE_SIM_PAD_UNIT];

static const struct bm_storage_info bm_storage_info = {
	.program_unit = 16,
	.erase_unit = 16,
	.wear_unit = 16,
	.erase_value = 0xFF,
	.is_erase_before_write = false,
};

static void event_send(const struct bm_storage *storage, struct bm_storage_evt *evt)
{
	if (!storage->evt_handler) {
		return;
	}

	storage->evt_handler(evt);
}

static void write_padded(const struct bm_storage *storage, uint32_t dest, const void *src,
			 uint32_t len)
{
	uint32_t aligned_len;
	uint32_t remainder;
	uint32_t pad_unit;
	void *dest_addr;

	dest_addr = (void *)(uintptr_t)(storage->addr + dest);
	pad_unit = storage->flags.is_wear_aligned ? bm_storage_info.wear_unit
						  : bm_storage_info.program_unit;
	aligned_len = ROUND_DOWN(len, pad_unit);

	memcpy(dest_addr, src, aligned_len);

	if (aligned_len < len) {
		__ASSERT_NO_MSG(storage->flags.is_write_padded);

		remainder = len - aligned_len;

		/* Copy the remaining bytes into the padding buffer */
		memcpy(pad_buffer, (const uint8_t *)src + aligned_len, remainder);
		/* Pad it with what's already stored in memory */
		memcpy(pad_buffer + remainder, (const uint8_t *)dest_addr + len,
		       pad_unit - remainder);
		memcpy((uint8_t *)dest_addr + aligned_len, pad_buffer, pad_unit);
	}
}

#if defined(CONFIG_BM_STORAGE_BACKEND_NATIVE_SIM_ASYNC)
#include <zephyr/kernel.h>

#define BM_STORAGE_NATIVE_SIM_STACK_SIZE 512
#define BM_STORAGE_NATIVE_SIM_PRIORITY	 5

K_THREAD_STACK_DEFINE(bm_storage_native_sim_stack_area, BM_STORAGE_NATIVE_SIM_STACK_SIZE);

static struct k_work_q bm_storage_native_sim_work_q;

static bool is_queue_init;

struct write_work_ctx {
	struct k_work_delayable work;
	const struct bm_storage *storage;
	uint32_t dest;
	const void *src;
	uint32_t len;
	void *ctx;
};

static void write_work_handler(struct k_work *work)
{
	struct write_work_ctx *work_ctx = CONTAINER_OF(work, struct write_work_ctx, work.work);

	if (work_ctx->storage->flags.is_write_padded &&
	    (work_ctx->len % (work_ctx->storage->flags.is_wear_aligned ?
			     bm_storage_info.wear_unit :
			     bm_storage_info.program_unit) != 0)) {
		write_padded(work_ctx->storage, work_ctx->dest, work_ctx->src, work_ctx->len);
	} else {
		memcpy((void *)(uintptr_t)(work_ctx->storage->addr + work_ctx->dest),
		       work_ctx->src, work_ctx->len);
	}

	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_WRITE_RESULT,
		.is_async = true,
		.result = 0,
		.addr = work_ctx->dest,
		.src = work_ctx->src,
		.len = work_ctx->len,
		.ctx = work_ctx->ctx,
	};

	event_send(work_ctx->storage, &evt);

	k_free(work_ctx);
}
#endif

static int bm_storage_native_sim_init(struct bm_storage *storage,
				      const struct bm_storage_config *config)
{
#if defined(CONFIG_BM_STORAGE_BACKEND_NATIVE_SIM_ASYNC)
	if (!is_queue_init) {
		k_work_queue_init(&bm_storage_native_sim_work_q);

		k_work_queue_start(&bm_storage_native_sim_work_q, bm_storage_native_sim_stack_area,
				K_THREAD_STACK_SIZEOF(bm_storage_native_sim_stack_area),
				BM_STORAGE_NATIVE_SIM_PRIORITY, NULL);
		is_queue_init = true;
	}
#endif

	storage->nvm_info = &bm_storage_info;
	return 0;
}

static int bm_storage_native_sim_uninit(struct bm_storage *storage)
{
	/* Nothing to do */
	return 0;
}

static int bm_storage_native_sim_read(const struct bm_storage *storage, uint32_t src, void *dest,
				      uint32_t len)
{
	memcpy(dest, (void *)(uintptr_t)(storage->addr + src), len);

	return 0;
}

static int bm_storage_native_sim_write(const struct bm_storage *storage, uint32_t dest,
				       const void *src, uint32_t len, void *ctx)
{
#if defined(CONFIG_BM_STORAGE_BACKEND_NATIVE_SIM_ASYNC)
	int err;

	struct write_work_ctx *work_ctx = k_malloc(sizeof(struct write_work_ctx));

	if (!work_ctx) {
		return -ENOMEM;
	}

	work_ctx->storage = storage;
	work_ctx->dest = dest;
	work_ctx->src = src;
	work_ctx->len = len;
	work_ctx->ctx = ctx;

	k_work_init_delayable(&work_ctx->work, write_work_handler);

	err = k_work_schedule_for_queue(&bm_storage_native_sim_work_q, &work_ctx->work,
					K_MSEC(100));
	if (err < 0) {
		return err;
	}

	return 0;
#else
	if (storage->flags.is_write_padded &&
	    (len % (storage->flags.is_wear_aligned ?
		   bm_storage_info.wear_unit :
		   bm_storage_info.program_unit) != 0)) {
		write_padded(storage, dest, src, len);
	} else {
		memcpy((void *)(uintptr_t)(storage->addr + dest), src, len);
	}

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
#endif
}

static int bm_storage_native_sim_erase(const struct bm_storage *storage, uint32_t addr,
				       uint32_t len, void *ctx)
{
	memset((void *)(uintptr_t)(storage->addr + addr), (int)(bm_storage_info.erase_value & 0xFF),
	       len);

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

const struct bm_storage_api bm_storage_native_sim_api = {
	.init = bm_storage_native_sim_init,
	.uninit = bm_storage_native_sim_uninit,
	.read = bm_storage_native_sim_read,
	.write = bm_storage_native_sim_write,
	.erase = bm_storage_native_sim_erase,
};
