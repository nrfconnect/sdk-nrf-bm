/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <stdint.h>
#include <bm/storage/bm_storage.h>
#include <bm/storage/bm_storage_backend.h>

static void event_send(const struct bm_storage *storage, struct bm_storage_evt *evt)
{
	if (!storage->evt_handler) {
		return;
	}

	storage->evt_handler(evt);
}

#if defined(CONFIG_BM_STORAGE_BACKEND_NATIVE_SIM_ASYNC)
#include <zephyr/kernel.h>

#define BM_STORAGE_NATIVE_SIM_STACK_SIZE 512
#define BM_STORAGE_NATIVE_SIM_PRIORITY	 5

K_THREAD_STACK_DEFINE(bm_storage_native_sim_stack_area, BM_STORAGE_NATIVE_SIM_STACK_SIZE);

static struct k_work_q bm_storage_native_sim_work_q;

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

	memcpy((void *)work_ctx->dest, work_ctx->src, work_ctx->len);

	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_WRITE_RESULT,
		.dispatch_type = BM_STORAGE_EVT_DISPATCH_ASYNC,
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

int bm_storage_backend_init(struct bm_storage *storage)
{
#if defined(CONFIG_BM_STORAGE_BACKEND_NATIVE_SIM_ASYNC)
	k_work_queue_init(&bm_storage_native_sim_work_q);

	k_work_queue_start(&bm_storage_native_sim_work_q, bm_storage_native_sim_stack_area,
			   K_THREAD_STACK_SIZEOF(bm_storage_native_sim_stack_area),
			   BM_STORAGE_NATIVE_SIM_PRIORITY, NULL);
#endif
	return 0;
}

int bm_storage_backend_read(const struct bm_storage *storage, uint32_t src, void *dest,
			    uint32_t len)
{
	memcpy(dest, (void *)src, len);

	return 0;
}

int bm_storage_backend_write(const struct bm_storage *storage, uint32_t dest, const void *src,
			     uint32_t len, void *ctx)
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
	memcpy((void *)dest, src, len);

	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_WRITE_RESULT,
		.dispatch_type = BM_STORAGE_EVT_DISPATCH_SYNC,
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

const struct bm_storage_info bm_storage_info = {
	.program_unit = 16,
	.no_explicit_erase = true
};
