/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <stdint.h>
#include <bm/storage/bm_storage.h>

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

#if defined(CONFIG_BM_STORAGE_BACKEND_NATIVE_SIM_ASYNC)
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#define BM_STORAGE_NATIVE_SIM_STACK_SIZE 512
#define BM_STORAGE_NATIVE_SIM_PRIORITY	 5

K_THREAD_STACK_DEFINE(bm_storage_native_sim_stack_area, BM_STORAGE_NATIVE_SIM_STACK_SIZE);

static struct k_work_q bm_storage_native_sim_work_q;

static atomic_t refcount;
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

	memcpy((void *)(uintptr_t)(work_ctx->storage->addr + work_ctx->dest),
	       work_ctx->src, work_ctx->len);

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
	atomic_inc(&refcount);

	if (atomic_get(&refcount) == 1 && !is_queue_init) {
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
#if defined(CONFIG_BM_STORAGE_BACKEND_NATIVE_SIM_ASYNC)
	if (atomic_get(&refcount) == 0) {
		return -EPERM;
	}

	if (atomic_dec(&refcount) == 1) {
		/* Last user: mark queue so next init will start it again. */
		is_queue_init = false;
	}
#endif
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
	memcpy((void *)(uintptr_t)(storage->addr + dest), src, len);

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
