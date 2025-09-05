/* Copyright (c) 2018 Laczen
 * Copyright (c) 2024 BayLibre SAS
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * BM_ZMS: Bare Metal Zephyr Memory Storage
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>

#include <bm_storage.h>
#include <bm_zms.h>
#include "bm_zms_priv.h"

#if CONFIG_HAS_NRFX
#include <nrfx.h>
#else
#define NRFX_CRITICAL_SECTION_ENTER(...)
#define NRFX_CRITICAL_SECTION_EXIT(...)
#endif

LOG_MODULE_REGISTER(bm_zms, CONFIG_BM_ZMS_LOG_LEVEL);

static zms_op_t cur_op; /* Current bm_zms operation. */
static zms_op_t *p_cur_op;
static atomic_t cur_op_result = ATOMIC_INIT(0);
static atomic_t queue_process_start = ATOMIC_INIT(false);

#ifdef CONFIG_BM_ZMS_LOOKUP_CACHE
static inline size_t zms_lookup_cache_pos(uint32_t id);
#endif
static void zms_event_handler(struct bm_storage_evt *evt);
static int zms_init(void);
static uint32_t zms_flash_al_wrt(struct bm_zms_fs *fs);
static int zms_write_execute(void);
static inline size_t zms_al_size(struct bm_zms_fs *fs, size_t len);
static uint32_t zms_flash_block_move(struct bm_zms_fs *fs);
static void zms_verify_space(zms_op_t *op);
static int bm_zms_clear_execute(void);

/* The number of queued operations.
 * Incremented by queue_start() and decremented by queue_has_next().
 */
static atomic_t queued_op_cnt;

/* The table of app callback functions. */
static bm_zms_cb_t zms_cb_table[CONFIG_BM_ZMS_MAX_USERS];

/* Queue of bm_zms operations. */
RING_BUF_DECLARE(zms_fifo, CONFIG_BM_ZMS_OP_QUEUE_SIZE * sizeof(zms_op_t));

static int zms_prev_ate(struct bm_zms_fs *fs, uint64_t *addr, struct zms_ate *ate);
static int zms_ate_valid(struct bm_zms_fs *fs, const struct zms_ate *entry);
static int zms_get_sector_cycle(struct bm_zms_fs *fs, uint64_t addr, uint8_t *cycle_cnt);
static int zms_get_sector_header(struct bm_zms_fs *fs, uint64_t addr, struct zms_ate *empty_ate,
				 struct zms_ate *close_ate);
static int zms_ate_valid_different_sector(struct bm_zms_fs *fs, const struct zms_ate *entry,
					  uint8_t cycle_cnt);

static void event_prepare(bm_zms_evt_t *p_evt)
{
	switch (cur_op.op_code) {
	case ZMS_OP_INIT:
		p_evt->evt_id = BM_ZMS_EVT_INIT;
		break;

	case ZMS_OP_WRITE:
		atomic_sub(&cur_op.fs->ongoing_writes, 1);
		p_evt->evt_id = BM_ZMS_EVT_WRITE;
		p_evt->id = cur_op.id;
		break;

	case ZMS_OP_CLEAR:
		p_evt->evt_id = BM_ZMS_EVT_CLEAR;
		break;

	case ZMS_OP_NONE:
		p_evt->evt_id = BM_ZMS_EVT_NONE;
		break;
	default:
		/* Should not happen. */
		break;
	}
}

static void event_send(bm_zms_evt_t const *const p_evt, struct bm_zms_fs *fs)
{
	if (zms_cb_table[fs->user_num] != NULL) {
		zms_cb_table[fs->user_num](p_evt);
	}
}

static bool queue_has_next(void)
{
	/** Decrement the number of queued operations. */
	if (queued_op_cnt != 0) {
		return atomic_sub(&queued_op_cnt, 1) == 1 ? false : true;
	}

	return false;
}

static void queue_process(void)
{
	int result = 0, prev_result = 0;
	int err, evt_result = 0;

	while (true) {
		if (queue_process_start) {
			/* If the storage operation has ended, reset the flag. */
			atomic_set(&queue_process_start, false);
		} else {
			/* We get here when the backend is asynchronous. */
			return;
		}
		prev_result = atomic_get(&cur_op_result);
		if (prev_result) {
			/* If the previous operation failed, we need to break the loop. */
			result = prev_result;
			goto completed;
		}

		if (p_cur_op == NULL) {
			/* Load the next from the queue if no operation is being executed.*/
			NRFX_CRITICAL_SECTION_ENTER();
			err = ring_buf_get(&zms_fifo, (uint8_t *)&cur_op, sizeof(zms_op_t));
			NRFX_CRITICAL_SECTION_EXIT();
			if (err < 0) {
				result = -EIO;
				goto completed;
			}
			p_cur_op = &cur_op;
		}

		/* We can reach here in three ways:
		 * from queue_start(): something was just queued
		 * from the BM_ZMS event handler: an operation is being executed
		 * looping: we only loop if there are operations still in the queue
		 *
		 * In all these three cases, cur_op != NULL.
		 */
		__ASSERT(p_cur_op != NULL, "p_cur_op is NULL, but it should not be.");

		switch (cur_op.op_code) {
		case ZMS_OP_INIT:
			result = zms_init();
			break;

		case ZMS_OP_WRITE:
			if ((cur_op.sub_step == ZMS_OP_WRITE_SUB_STEP_ATE2) ||
			    (cur_op.sub_step == ZMS_OP_WRITE_SUB_STEP_DATA2)) {
				/* If we are in the second sub-step, we need to write the second
				 * part.
				 */
				result = zms_flash_al_wrt(cur_op.fs);
			} else if ((cur_op.gc.step == ZMS_OP_WRITE_GC_BLK_MOVE) &&
				   (cur_op.gc.blk_mv_len)) {
				/* If we are still moving data, a previous block write succeeded .
				 * Increase the data write address by the size of the block.
				 */
				cur_op.fs->data_wra += zms_al_size(cur_op.fs, ZMS_BLOCK_SIZE);
				/* If we are in the garbage collection step, we need to move the
				 * block.
				 */
				result = zms_flash_block_move(cur_op.fs);
			} else if (cur_op.step == ZMS_OP_WRITE_STARTUP) {
				zms_verify_space(&cur_op);
				result = zms_write_execute();
			} else {
				result = zms_write_execute();
			}
			break;
		case ZMS_OP_CLEAR:
			if (cur_op.step == ZMS_OP_CLEAR_DONE) {
				/* bm_zms needs to be reinitialized after clearing */
				cur_op.fs->init_flags.initialized = false;
				cur_op.fs->init_flags.initializing = false;
				zms_cb_table[cur_op.fs->user_num] = NULL;
				cur_op.op_completed = true;
				result = 0;
			} else {
				result = bm_zms_clear_execute();
			}
			break;

		default:
			result = -EIO;
			break;
		}

		if (!result && !cur_op.op_completed) {
			continue;
		}

completed:
		/* The operation has completed (either successfully or with an error).
		 * - send an event to the user
		 * - free the operation buffer
		 * - execute any other queued operations
		 */
		cur_op.op_completed = true;

		if (result > 0) {
			/* If result is not 0, an internal error occurred. */
			evt_result = -EIO;
		} else if (result < 0) {
			/* propagate the negative errno to the event handler. */
			evt_result = result;
		} else {
			/* no errors. */
			evt_result = 0;
		}
		bm_zms_evt_t evt = {
			/* The operation might have failed for one of the following reasons:
			 * -ENOSPC:  no free space in flash.
			 * -EIO:     Internal BM_ZMS error.
			 * -ENOTSUP: BM_ZMS version not supported.
			 * -ENOEXEC: Bad BM_ZMS format.
			 * -EFAULT:  Bad sector layout.
			 * -ENOMEM:  internal queue buffer is full.
			 */
			.result = evt_result,
		};

		if ((cur_op.op_code == ZMS_OP_INIT) && cur_op.op_completed) {
			/* print information about sectors layout after init. */
			LOG_INF("%u Sectors of %u bytes", cur_op.fs->sector_count,
				cur_op.fs->sector_size);
			LOG_INF("alloc wra: %llu, %llx", SECTOR_NUM(cur_op.fs->ate_wra),
				SECTOR_OFFSET(cur_op.fs->ate_wra));
			LOG_INF("data wra: %llu, %llx", SECTOR_NUM(cur_op.fs->data_wra),
				SECTOR_OFFSET(cur_op.fs->data_wra));
		}

		event_prepare(&evt);
		event_send(&evt, cur_op.fs);

		/* Zero the pointer to the current operation so that this function
		 * will fetch a new one from the queue next time it is run.
		 */
		p_cur_op = NULL;

		/* The result of the operation must be reset upon re-entering the loop to ensure
		 * the next operation won't be affected by eventual errors in previous operations.
		 */
		result = 0;

		if (!queue_has_next()) {
			LOG_DBG("No more elements in the queue, exiting.");
			/* No more elements left. Nothing to do. */
			break;
		}
		LOG_DBG("There are more elements in the queue, processing next one. %u",
			(uint32_t)queued_op_cnt);
		atomic_set(&queue_process_start, true);
	}
}

static void queue_start(void)
{
	if (!atomic_add(&queued_op_cnt, 1)) {
		atomic_set(&queue_process_start, true);
		queue_process();
	}
}

static inline bool is_end_of_ate_write_step(zms_op_t *p_op)
{
	if ((p_op->step == ZMS_OP_WRITE_DONE) ||
	    (p_op->step == ZMS_OP_INIT_DONE) ||
	    (p_op->step == ZMS_OP_WRITE_CLOSE_SECTOR_ATE) ||
	    (p_op->step == ZMS_OP_WRITE_CLOSE_SECTOR_DONE) ||
	    (p_op->gc.step == ZMS_OP_WRITE_GC_EXECUTE) ||
	    (p_op->gc.step == ZMS_OP_WRITE_GC_ATE_COPY_DONE) ||
	    (((p_op->step == ZMS_OP_WRITE_GC) || (p_op->step == ZMS_OP_INIT_GC)) &&
	    (p_op->gc.step == ZMS_OP_WRITE_GC_DONE_EMPTY_SECTOR))) {
		return true;
	}

	return false;
}

static void zms_event_handler(struct bm_storage_evt *p_evt)
{
	zms_op_t *p_op;

	if (p_evt->ctx != NULL) {
		p_op = (zms_op_t *)(p_evt->ctx);

		if (is_end_of_ate_write_step(p_op)) {
#ifdef CONFIG_BM_ZMS_LOOKUP_CACHE
			/* 0xFFFFFFFF is a special-purpose identifier. Exclude it from the cache */
			if (p_op->ate_entry.id != ZMS_HEAD_ID) {
				p_op->fs->lookup_cache[zms_lookup_cache_pos(p_op->ate_entry.id)] =
					p_op->fs->ate_wra;
			}
#endif
			p_op->fs->ate_wra -= zms_al_size(p_op->fs, sizeof(struct zms_ate));
		}
	} else {
		/* Should never happen. */
		LOG_ERR("%s: p_evt->ctx is NULL", __func__);
		return;
	}
	atomic_set(&queue_process_start, true);
	atomic_set(&cur_op_result, p_evt->result);

	if (p_evt->dispatch_type == BM_STORAGE_EVT_DISPATCH_ASYNC) {
		queue_process();
	}
}

int bm_zms_register(struct bm_zms_fs *fs, bm_zms_cb_t cb)
{
	int i;

	if (!fs || !cb) {
		return -EINVAL;
	}

	for (i = 0; i < CONFIG_BM_ZMS_MAX_USERS; i++) {
		if (zms_cb_table[i] == NULL) {
			break;
		}
	}
	if (i == CONFIG_BM_ZMS_MAX_USERS) {
		return -ENOMEM;
	}
	zms_cb_table[i] = cb;
	fs->user_num = i;
	fs->init_flags.cb_registred = true;

	return 0;
}

#ifdef CONFIG_BM_ZMS_LOOKUP_CACHE

static inline size_t zms_lookup_cache_pos(uint32_t id)
{
	uint32_t hash;

	/* 32-bit integer hash function found by https://github.com/skeeto/hash-prospector. */
	hash = id;
	hash ^= hash >> 16;
	hash *= 0x7feb352dU;
	hash ^= hash >> 15;
	hash *= 0x846ca68bU;
	hash ^= hash >> 16;

	return hash % CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE;
}

static int zms_lookup_cache_rebuild(struct bm_zms_fs *fs)
{
	int rc;
	int previous_sector_num = ZMS_INVALID_SECTOR_NUM;
	uint64_t addr;
	uint64_t ate_addr;
	uint64_t *cache_entry;
	uint8_t current_cycle;
	struct zms_ate ate;

	memset(fs->lookup_cache, 0xff, sizeof(fs->lookup_cache));
	addr = fs->ate_wra;

	while (true) {
		/* Make a copy of 'addr' as it will be advanced by zms_prev_ate() */
		ate_addr = addr;
		rc = zms_prev_ate(fs, &addr, &ate);

		if (rc) {
			return rc;
		}

		cache_entry = &fs->lookup_cache[zms_lookup_cache_pos(ate.id)];

		if (ate.id != ZMS_HEAD_ID && *cache_entry == ZMS_LOOKUP_CACHE_NO_ADDR) {
			/* read the ate cycle only when we change the sector
			 * or if it is the first read
			 */
			if (SECTOR_NUM(ate_addr) != previous_sector_num) {
				rc = zms_get_sector_cycle(fs, ate_addr, &current_cycle);
				if (rc == -ENOENT) {
					/* sector never used */
					current_cycle = 0;
				} else if (rc) {
					/* bad flash read */
					return rc;
				}
			}
			if (zms_ate_valid_different_sector(fs, &ate, current_cycle)) {
				*cache_entry = ate_addr;
			}
			previous_sector_num = SECTOR_NUM(ate_addr);
		}

		if (addr == fs->ate_wra) {
			break;
		}
	}

	return 0;
}

static void zms_lookup_cache_invalidate(struct bm_zms_fs *fs, uint32_t sector)
{
	uint64_t *cache_entry = fs->lookup_cache;
	uint64_t *const cache_end = &fs->lookup_cache[CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE];

	for (; cache_entry < cache_end; ++cache_entry) {
		if (SECTOR_NUM(*cache_entry) == sector) {
			*cache_entry = ZMS_LOOKUP_CACHE_NO_ADDR;
		}
	}
}

#endif /* CONFIG_BM_ZMS_LOOKUP_CACHE */

/* Helper to compute offset given the address */
static inline off_t zms_addr_to_offset(struct bm_zms_fs *fs, uint64_t addr)
{
	return fs->offset + (fs->sector_size * SECTOR_NUM(addr)) + SECTOR_OFFSET(addr);
}

/* Helper to round down len to the closest multiple of write_block_size  */
static inline size_t zms_round_down_write_block_size(struct bm_zms_fs *fs, size_t len)
{
	return len & ~(fs->zms_bm_storage.nvm_info->program_unit - 1U);
}

/* Helper to round up len to multiple of write_block_size */
static inline size_t zms_round_up_write_block_size(struct bm_zms_fs *fs, size_t len)
{
	return (len + (fs->zms_bm_storage.nvm_info->program_unit - 1U)) &
	       ~(fs->zms_bm_storage.nvm_info->program_unit - 1U);
}

/* zms_al_size returns size aligned to fs->write_block_size */
static inline size_t zms_al_size(struct bm_zms_fs *fs, size_t len)
{
	size_t write_block_size = fs->zms_bm_storage.nvm_info->program_unit;

	if (write_block_size <= 1U) {
		return len;
	}

	return zms_round_up_write_block_size(fs, len);
}

/* Helper to get empty ATE address */
static inline uint64_t zms_empty_ate_addr(struct bm_zms_fs *fs, uint64_t addr)
{
	return (addr & ADDR_SECT_MASK) + fs->sector_size - fs->ate_size;
}

/* Helper to get close ATE address */
static inline uint64_t zms_close_ate_addr(struct bm_zms_fs *fs, uint64_t addr)
{
	return (addr & ADDR_SECT_MASK) + fs->sector_size - 2 * fs->ate_size;
}

static void zms_next_state_common(zms_write_step_t next_step, zms_write_step_t default_state)
{
	switch (cur_op.sub_step) {
	case ZMS_OP_WRITE_SUB_STEP_ATE1:
		if (cur_op.len) {
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_ATE2;
			cur_op.addr = cur_op.fs->ate_wra;
		} else {
			cur_op.step = next_step;
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		}
		break;
	case ZMS_OP_WRITE_SUB_STEP_ATE2:
		cur_op.step = next_step;
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		break;
	default:
		/* Should not happen */
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		cur_op.step = default_state;
		break;
	}
}

static void zms_next_state_write_execute(void)
{
	switch (cur_op.sub_step) {
	case ZMS_OP_WRITE_SUB_STEP_DATA1:
		if (cur_op.len) {
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_DATA2;
			cur_op.addr = cur_op.fs->data_wra;
		} else {
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_ATE1;
		}
		break;
	case ZMS_OP_WRITE_SUB_STEP_DATA2:
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_ATE1;
		break;
	case ZMS_OP_WRITE_SUB_STEP_ATE1:
		if (cur_op.len) {
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_ATE2;
			cur_op.addr = cur_op.fs->ate_wra;
		} else {
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
			cur_op.step = ZMS_OP_WRITE_DONE;
		}
		break;
	case ZMS_OP_WRITE_SUB_STEP_ATE2:
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		cur_op.step = ZMS_OP_WRITE_DONE;
		break;
	default:
		/* Should not happen */
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		break;
	}
}

static void zms_next_state_write_close_sector_garbage(void)
{
	zms_next_state_common(ZMS_OP_WRITE_CLOSE_SECTOR_ATE, ZMS_OP_WRITE_DONE);
}

static void zms_next_state_write_close_sector_ate(void)
{
	zms_next_state_common(ZMS_OP_WRITE_CLOSE_SECTOR_DONE, ZMS_OP_WRITE_DONE);
}

static void zms_next_state_gc(void)
{
	switch (cur_op.gc.step) {
	case ZMS_OP_WRITE_GC_INIT:
		cur_op.gc.step = ZMS_OP_WRITE_GC_EXECUTE;
		break;
	case ZMS_OP_WRITE_GC_INIT_EMPTY_SECTOR:
		cur_op.gc.step = ZMS_OP_WRITE_GC_INIT;
		break;
	case ZMS_OP_WRITE_GC_EXECUTE:
		if ((cur_op.gc.blk_mv_len) && (cur_op.ate_entry.len > ZMS_DATA_IN_ATE_SIZE)) {
			cur_op.gc.step = ZMS_OP_WRITE_GC_BLK_MOVE;
		} else {
			cur_op.gc.step = ZMS_OP_WRITE_GC_ATE_COPY;
		}
		break;
	case ZMS_OP_WRITE_GC_BLK_MOVE:
		if (!cur_op.gc.blk_mv_len) {
			/* If there is no more data to move, we can proceed to the next step */
			cur_op.gc.step = ZMS_OP_WRITE_GC_ATE_COPY;
		} else {
			/* If there is still data to move, we need to continue moving it */
			cur_op.gc.step = ZMS_OP_WRITE_GC_BLK_MOVE;
		}
		break;
	case ZMS_OP_WRITE_GC_ATE_COPY:
		if (cur_op.gc.gc_prev_addr == cur_op.gc.stop_addr) {
			cur_op.gc.step = ZMS_OP_WRITE_GC_ATE_COPY_DONE;
		} else {
			cur_op.gc.step = ZMS_OP_WRITE_GC_EXECUTE;
		}
		break;
	case ZMS_OP_WRITE_GC_DONE:
	case ZMS_OP_WRITE_GC_ATE_COPY_DONE:
		cur_op.gc.step = ZMS_OP_WRITE_GC_DONE_EMPTY_SECTOR;
		break;
	case ZMS_OP_WRITE_GC_DONE_EMPTY_SECTOR:
		if (cur_op.op_code == ZMS_OP_WRITE) {
			cur_op.data = cur_op.app_data;
			zms_verify_space(&cur_op);
			if (cur_op.step == ZMS_OP_WRITE_EXECUTE) {
				cur_op.gc.step = ZMS_OP_WRITE_GC_NONE;
				cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
			}
		} else if (cur_op.op_code == ZMS_OP_INIT) {
			cur_op.step = ZMS_OP_INIT_ADD_GC_DONE;
			cur_op.gc.step = ZMS_OP_WRITE_GC_NONE;
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		}
		break;

	default:
		/* Should not happen */
		cur_op.gc.step = ZMS_OP_WRITE_GC_NONE;
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		cur_op.step = ZMS_OP_WRITE_DONE;
		break;
	}
}

static void zms_next_state_common_gc(void)
{
	switch (cur_op.sub_step) {
	case ZMS_OP_WRITE_SUB_STEP_ATE1:
		if (cur_op.len) {
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_ATE2;
			cur_op.addr = cur_op.fs->ate_wra;
		} else {
			zms_next_state_gc();
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		}
		break;
	case ZMS_OP_WRITE_SUB_STEP_ATE2:
		zms_next_state_gc();
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		break;
	case ZMS_OP_WRITE_SUB_STEP_DATA1:
		if (cur_op.len) {
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_DATA2;
			cur_op.addr = cur_op.fs->data_wra;
		} else {
			zms_next_state_gc();
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		}
		break;
	case ZMS_OP_WRITE_SUB_STEP_DATA2:
		zms_next_state_gc();
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		break;
	default: /* Should not happen */
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_NONE;
		cur_op.step = ZMS_OP_WRITE_DONE;
		break;
	}
}

static void zms_next_state_init_all_open_add_empty_ate(void)
{
	zms_next_state_common(ZMS_OP_INIT_ALL_OPEN_ADD_EMPTY_ATE, ZMS_OP_INIT_DONE);
}

static void zms_next_state_init_add_empty_ate_gc_done(void)
{
	zms_next_state_common(ZMS_OP_INIT_ADD_GC_DONE, ZMS_OP_INIT_DONE);
}

static void zms_next_state_init_add_empty_ate_gc_todo(void)
{
	zms_next_state_common(ZMS_OP_INIT_GC_START, ZMS_OP_INIT_DONE);
}

static void zms_next_state_init_add_gc_done(void)
{
	zms_next_state_common(ZMS_OP_INIT_DONE, ZMS_OP_INIT_DONE);
}

static void zms_next_state_clear(void)
{
	if (cur_op.clear_sector >= cur_op.fs->sector_count) {
		zms_next_state_common(ZMS_OP_CLEAR_DONE, ZMS_OP_CLEAR_DONE);
	} else {
		zms_next_state_common(ZMS_OP_CLEAR_EXECUTE, ZMS_OP_CLEAR_DONE);
	}
}

static void zms_al_wrt_next_op(struct bm_zms_fs *fs)
{
	if (cur_op.op_code == ZMS_OP_WRITE) {
		switch (cur_op.step) {
		case ZMS_OP_WRITE_EXECUTE:
			zms_next_state_write_execute();
			break;
		case ZMS_OP_WRITE_CLOSE_SECTOR_GARBAGE:
			zms_next_state_write_close_sector_garbage();
			break;
		case ZMS_OP_WRITE_CLOSE_SECTOR_ATE:
			zms_next_state_write_close_sector_ate();
			break;
		case ZMS_OP_WRITE_GC:
			zms_next_state_common_gc();
			break;
		case ZMS_OP_WRITE_DONE:
			break;
		default:
			/* Should not happen */
			cur_op.step = ZMS_OP_WRITE_DONE;
			break;
		}
	} else if (cur_op.op_code == ZMS_OP_INIT) {
		switch (cur_op.step) {
		case ZMS_OP_INIT_ALL_OPEN_ADD_EMPTY_ATE:
			zms_next_state_init_all_open_add_empty_ate();
			break;
		case ZMS_OP_INIT_ADD_EMPTY_ATE_GC_DONE:
			zms_next_state_init_add_empty_ate_gc_done();
			break;
		case ZMS_OP_INIT_ADD_EMPTY_ATE_GC_TODO:
			zms_next_state_init_add_empty_ate_gc_todo();
			break;
		case ZMS_OP_INIT_ADD_GC_DONE:
			zms_next_state_init_add_gc_done();
			break;
		case ZMS_OP_INIT_GC_START:
		case ZMS_OP_INIT_GC:
			zms_next_state_common_gc();
			break;
		case ZMS_OP_INIT_DONE:
			fs->init_flags.initializing = false;
			fs->init_flags.initialized = true;
			break;
		default:
			/* Should not happen */
			cur_op.step = ZMS_OP_INIT_DONE;
			fs->init_flags.initializing = false;
			fs->init_flags.initialized = true;
			break;
		}
	} else if (cur_op.op_code == ZMS_OP_CLEAR) {
		zms_next_state_clear();
	}
}

/* Aligned memory write */
static uint32_t zms_flash_al_wrt(struct bm_zms_fs *fs)
{
	const uint8_t *data8;
	uint32_t rc = 0;
	off_t offset;
	uint8_t buf[ZMS_BLOCK_SIZE];

	if (!cur_op.len) {
		zms_al_wrt_next_op(fs);
		/* Nothing to write, avoid changing the flash protection */
		return 0;
	}

	if ((cur_op.sub_step == ZMS_OP_WRITE_SUB_STEP_ATE1) ||
	    (cur_op.sub_step == ZMS_OP_WRITE_SUB_STEP_ATE2)) {
		/* If this is an ATE write set the data8 pointer to the ATE. */
		data8 = (const uint8_t *)&cur_op.ate_entry;
	} else {
		/* If this is a data write set the data8 pointer to the actual data to write.
		 * This could be either a data from the App or from an internal buffer.
		 */
		data8 = (const uint8_t *)cur_op.data;
	}
	offset = zms_addr_to_offset(fs, cur_op.addr);

	cur_op.blen = zms_round_down_write_block_size(fs, cur_op.len);
	if (cur_op.blen > 0) {
		cur_op.len -= cur_op.blen;
		zms_al_wrt_next_op(fs);
		return bm_storage_write(&fs->zms_bm_storage, offset, data8, cur_op.blen,
					(void *)&cur_op);
	}
	if (cur_op.len) {
		memcpy(buf, data8 + cur_op.blen, cur_op.len);
		(void)memset(buf + cur_op.len, fs->zms_bm_storage.nvm_info->erase_value,
			     fs->zms_bm_storage.nvm_info->program_unit - cur_op.len);
		cur_op.len = 0;
		zms_al_wrt_next_op(fs);
		return bm_storage_write(&fs->zms_bm_storage, offset + cur_op.blen, buf,
					fs->zms_bm_storage.nvm_info->program_unit,
					(void *)&cur_op);
	}

	return rc;
}

/* basic flash read from bm_zms address */
static int zms_flash_rd(struct bm_zms_fs *fs, uint64_t addr, void *data, size_t len)
{
	off_t offset;

	offset = zms_addr_to_offset(fs, addr);

	return bm_storage_read(&fs->zms_bm_storage, offset, data, len);
}

/* allocation entry write */
static uint32_t zms_flash_ate_wrt(struct bm_zms_fs *fs)
{
	if (cur_op.sub_step == ZMS_OP_WRITE_SUB_STEP_NONE) {
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_ATE1;
		cur_op.len = sizeof(struct zms_ate);
	}
	return zms_flash_al_wrt(fs);
}

/* data write */
static int zms_flash_data_wrt(struct bm_zms_fs *fs)
{
	if (cur_op.sub_step == ZMS_OP_WRITE_SUB_STEP_NONE) {
		cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_DATA1;
	}
	if (cur_op.sub_step == ZMS_OP_WRITE_SUB_STEP_DATA1) {
		cur_op.len = cur_op.data_len;
	}
	cur_op.addr = fs->data_wra;

	return zms_flash_al_wrt(fs);
}

/* flash ate read */
static int zms_flash_ate_rd(struct bm_zms_fs *fs, uint64_t addr, struct zms_ate *entry)
{
	return zms_flash_rd(fs, addr, entry, sizeof(struct zms_ate));
}

/* zms_flash_block_cmp compares the data in flash at addr to data
 * in blocks of size ZMS_BLOCK_SIZE aligned to fs->write_block_size
 * returns 0 if equal, 1 if not equal, errcode if error
 */
static int zms_flash_block_cmp(struct bm_zms_fs *fs, uint64_t addr, const void *data, size_t len)
{
	const uint8_t *data8 = (const uint8_t *)data;
	int rc;
	size_t bytes_to_cmp;
	size_t block_size;
	uint8_t buf[ZMS_BLOCK_SIZE];

	block_size = zms_round_down_write_block_size(fs, ZMS_BLOCK_SIZE);

	while (len) {
		bytes_to_cmp = MIN(block_size, len);
		rc = zms_flash_rd(fs, addr, buf, bytes_to_cmp);
		if (rc < 0) {
			return rc;
		}
		rc = memcmp(data8, buf, bytes_to_cmp);
		if (rc) {
			return 1;
		}
		len -= bytes_to_cmp;
		addr += bytes_to_cmp;
		data8 += bytes_to_cmp;
	}
	return 0;
}

/* zms_flash_cmp_const compares the data in flash at addr to a constant
 * value. returns 0 if all data in flash is equal to value, 1 if not equal,
 * errcode if error
 */
static int zms_flash_cmp_const(struct bm_zms_fs *fs, uint64_t addr, uint8_t value, size_t len)
{
	int rc;
	size_t bytes_to_cmp;
	size_t block_size;
	uint8_t cmp[ZMS_BLOCK_SIZE];

	block_size = zms_round_down_write_block_size(fs, ZMS_BLOCK_SIZE);

	(void)memset(cmp, value, block_size);
	while (len) {
		bytes_to_cmp = MIN(block_size, len);
		rc = zms_flash_block_cmp(fs, addr, cmp, bytes_to_cmp);
		if (rc) {
			return rc;
		}
		len -= bytes_to_cmp;
		addr += bytes_to_cmp;
	}
	return 0;
}

/* flash block move: move a block at addr to the current data write location
 * and updates the data write location.
 */
static uint32_t zms_flash_block_move(struct bm_zms_fs *fs)
{
	int rc;
	size_t bytes_to_copy;
	size_t block_size;

	static __aligned(4) uint8_t buf_gc[ZMS_BLOCK_SIZE];

	block_size = zms_round_down_write_block_size(fs, ZMS_BLOCK_SIZE);

	if (cur_op.gc.blk_mv_len) {
		bytes_to_copy = MIN(block_size, cur_op.gc.blk_mv_len);
		rc = zms_flash_rd(fs, cur_op.gc.blk_mv_addr, buf_gc, bytes_to_copy);
		if (rc) {
			return rc;
		}

		cur_op.gc.blk_mv_len -= bytes_to_copy;
		cur_op.gc.blk_mv_addr += bytes_to_copy;
		cur_op.data = buf_gc;
		cur_op.data_len = bytes_to_copy;
		cur_op.len = bytes_to_copy;
		return zms_flash_data_wrt(fs);
	}
	return 0;
}

/* erase a sector and verify erase was OK.
 * return 0 if OK, errorcode on error.
 */
static int zms_flash_erase_sector(struct bm_zms_fs *fs, uint64_t addr)
{
	uint32_t rc;
	off_t offset;
	bool ebw_required = !fs->zms_bm_storage.nvm_info->no_explicit_erase;

	if (!ebw_required) {
		/* Do nothing for devices that do not have erase capability */
		return 0;
	}

	/* Currently the devices that need explicit erase are not supported. */
	return -ENOTSUP;

	/* This is a placeholder for the erase operation that will be enabled once the
	 * BM storage API supports it.
	 */
	addr &= ADDR_SECT_MASK;
	offset = zms_addr_to_offset(fs, addr);

	LOG_DBG("Erasing flash at offset 0x%lx ( 0x%llx ), len %u", (long)offset, addr,
		fs->sector_size);

#ifdef CONFIG_BM_ZMS_LOOKUP_CACHE
	zms_lookup_cache_invalidate(fs, SECTOR_NUM(addr));
#endif
	rc = bm_storage_erase(&fs->zms_bm_storage, offset, fs->sector_size, &cur_op);

	if (rc) {
		return rc;
	}

	if (zms_flash_cmp_const(fs, addr, fs->zms_bm_storage.nvm_info->erase_value,
				fs->sector_size)) {
		LOG_ERR("Failure while erasing the sector at offset 0x%lx", (long)offset);
		rc = -EIO;
	}

	return rc;
}

/* crc update on allocation entry */
static void zms_ate_crc8_update(struct zms_ate *entry)
{
	uint8_t crc8;

	/* crc8 field is the first element of the structure, do not include it */
	crc8 = crc8_ccitt(0xff, (uint8_t *)entry + SIZEOF_FIELD(struct zms_ate, crc8),
			  sizeof(struct zms_ate) - SIZEOF_FIELD(struct zms_ate, crc8));
	entry->crc8 = crc8;
}

/* crc check on allocation entry
 * returns 0 if OK, 1 on crc fail
 */
static int zms_ate_crc8_check(const struct zms_ate *entry)
{
	uint8_t crc8;

	/* crc8 field is the first element of the structure, do not include it */
	crc8 = crc8_ccitt(0xff, (uint8_t *)entry + SIZEOF_FIELD(struct zms_ate, crc8),
			  sizeof(struct zms_ate) - SIZEOF_FIELD(struct zms_ate, crc8));
	if (crc8 == entry->crc8) {
		return 0;
	}

	return 1;
}

/* zms_ate_valid validates an ate in the current sector by checking if the ate crc is valid
 * and its cycle cnt matches the cycle cnt of the active sector
 *
 * return 1 if ATE is valid,
 *        0 otherwise
 *
 * see: zms_ate_valid_different_sector
 */
static int zms_ate_valid(struct bm_zms_fs *fs, const struct zms_ate *entry)
{
	return zms_ate_valid_different_sector(fs, entry, fs->sector_cycle);
}

/* zms_ate_valid_different_sector validates an ate that is in a different
 * sector than the active one. It takes as argument the cycle_cnt of the
 * sector where the ATE to be validated is stored
 *     return 1 if crc8 and cycle_cnt are valid,
 *            0 otherwise
 */
static int zms_ate_valid_different_sector(struct bm_zms_fs *fs, const struct zms_ate *entry,
					  uint8_t cycle_cnt)
{
	if ((cycle_cnt != entry->cycle_cnt) || zms_ate_crc8_check(entry)) {
		return 0;
	}

	return 1;
}

static inline int zms_get_cycle_on_sector_change(struct bm_zms_fs *fs, uint64_t addr,
						 int previous_sector_num, uint8_t *cycle_cnt)
{
	int rc;

	/* read the ate cycle only when we change the sector
	 * or if it is the first read
	 */
	if (SECTOR_NUM(addr) != previous_sector_num) {
		rc = zms_get_sector_cycle(fs, addr, cycle_cnt);
		if (rc == -ENOENT) {
			/* sector never used */
			*cycle_cnt = 0;
		} else if (rc) {
			/* bad flash read */
			return rc;
		}
	}

	return 0;
}

/* zms_close_ate_valid validates a sector close ate.
 * A valid sector close ate should be:
 * - a valid ate
 * - with len = 0 and id = ZMS_HEAD_ID
 * - and offset points to location at ate multiple from sector size
 * return true if valid, false otherwise
 */
static bool zms_close_ate_valid(struct bm_zms_fs *fs, const struct zms_ate *entry)
{
	return (zms_ate_valid_different_sector(fs, entry, entry->cycle_cnt) && (!entry->len) &&
		(entry->id == ZMS_HEAD_ID) && !((fs->sector_size - entry->offset) % fs->ate_size));
}

/* zms_empty_ate_valid validates an sector empty ate.
 * A valid sector empty ate should be:
 * - a valid ate
 * - with len = 0xffff and id = 0xffffffff
 * return true if valid, false otherwise
 */
static bool zms_empty_ate_valid(struct bm_zms_fs *fs, const struct zms_ate *entry)
{
	return (zms_ate_valid_different_sector(fs, entry, entry->cycle_cnt) &&
		(entry->len == 0xffff) && (entry->id == ZMS_HEAD_ID));
}

/* zms_gc_done_ate_valid validates a garbage collector done ATE
 * Valid gc_done_ate:
 * - valid ate
 * - len = 0
 * - id = 0xffffffff
 * return true if valid, false otherwise
 */
static bool zms_gc_done_ate_valid(struct bm_zms_fs *fs, const struct zms_ate *entry)
{
	return (zms_ate_valid_different_sector(fs, entry, entry->cycle_cnt) && (!entry->len) &&
		(entry->id == ZMS_HEAD_ID));
}

/* Read empty and close ATE of the sector where belongs address "addr" and
 * validates that the sector is closed.
 * retval: 0 if sector is not close
 * retval: 1 is sector is closed
 * retval: < 0 if read of the header failed.
 */
static int zms_validate_closed_sector(struct bm_zms_fs *fs, uint64_t addr,
				      struct zms_ate *empty_ate, struct zms_ate *close_ate)
{
	int rc;

	/* read the header ATEs */
	rc = zms_get_sector_header(fs, addr, empty_ate, close_ate);
	if (rc) {
		return -EIO;
	}

	if (zms_empty_ate_valid(fs, empty_ate) && zms_close_ate_valid(fs, close_ate) &&
	    (empty_ate->cycle_cnt == close_ate->cycle_cnt)) {
		/* Closed sector validated */
		return 1;
	}

	return 0;
}

/* store an entry in flash */
static int zms_flash_write_entry(struct bm_zms_fs *fs)
{
	if (cur_op.sub_step == ZMS_OP_WRITE_SUB_STEP_NONE) {
		if (cur_op.data_len > ZMS_DATA_IN_ATE_SIZE) {
			/* data_len is greater than 8 bytes, write data separately */
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_DATA1;
		} else {
			/* data_len is less than or equal to 8 bytes, write data in ATE */
			cur_op.sub_step = ZMS_OP_WRITE_SUB_STEP_ATE1;
		}
	}

	if ((cur_op.sub_step == ZMS_OP_WRITE_SUB_STEP_ATE1) ||
	    (cur_op.sub_step == ZMS_OP_WRITE_SUB_STEP_ATE2)) {
		/* Initialize all members to 0 */
		memset(&cur_op.ate_entry, 0, sizeof(struct zms_ate));

		cur_op.ate_entry.id = cur_op.id;
		cur_op.ate_entry.len = (uint16_t)cur_op.data_len;
		cur_op.ate_entry.cycle_cnt = fs->sector_cycle;

		if (cur_op.data_len > ZMS_DATA_IN_ATE_SIZE) {
			/* only compute CRC if len is greater than 8 bytes */
			if (IS_ENABLED(CONFIG_BM_ZMS_DATA_CRC)) {
				cur_op.ate_entry.data_crc =
					crc32_ieee(cur_op.data, cur_op.data_len);
			}
			cur_op.ate_entry.offset = (uint32_t)SECTOR_OFFSET(fs->data_wra);
		} else if ((cur_op.data_len > 0) && (cur_op.data_len <= ZMS_DATA_IN_ATE_SIZE)) {
			/* Copy data into entry for small data ( < 8B) */
			memcpy(&cur_op.ate_entry.data, cur_op.data, cur_op.data_len);
		}
		zms_ate_crc8_update(&cur_op.ate_entry);
	}

	switch (cur_op.sub_step) {
	case ZMS_OP_WRITE_SUB_STEP_DATA1:
	case ZMS_OP_WRITE_SUB_STEP_DATA2:
		cur_op.addr = fs->data_wra;
		return zms_flash_data_wrt(fs);
	case ZMS_OP_WRITE_SUB_STEP_ATE1:
	case ZMS_OP_WRITE_SUB_STEP_ATE2:
		cur_op.addr = fs->ate_wra;
		cur_op.len = sizeof(struct zms_ate);
		return zms_flash_ate_wrt(fs);
	default:
		break;
	}

	return 0;
}

/* end of flash routines */

/* Search for the last valid ATE written in a sector and also update data write address
 */
static int zms_recover_last_ate(struct bm_zms_fs *fs, uint64_t *addr, uint64_t *data_wra)
{
	uint64_t data_end_addr;
	uint64_t ate_end_addr;
	struct zms_ate end_ate;
	int rc;

	LOG_DBG("Recovering last ate from sector %llu", SECTOR_NUM(*addr));

	/* skip close and empty ATE */
	*addr -= fs->ate_size;

	ate_end_addr = *addr;
	data_end_addr = *addr & ADDR_SECT_MASK;
	/* Initialize the data_wra to the first address of the sector */
	*data_wra = data_end_addr;

	while (ate_end_addr > data_end_addr) {
		rc = zms_flash_ate_rd(fs, ate_end_addr, &end_ate);
		if (rc) {
			return rc;
		}
		if (zms_ate_valid(fs, &end_ate)) {
			/* found a valid ate, update data_end_addr and *addr */
			data_end_addr &= ADDR_SECT_MASK;
			if (end_ate.len > ZMS_DATA_IN_ATE_SIZE) {
				data_end_addr += end_ate.offset + zms_al_size(fs, end_ate.len);
				*data_wra = data_end_addr;
			}
			*addr = ate_end_addr;
		}
		ate_end_addr -= fs->ate_size;
	}

	return 0;
}

/* compute previous addr of ATE */
static int zms_compute_prev_addr(struct bm_zms_fs *fs, uint64_t *addr)
{
	int sec_closed;
	struct zms_ate empty_ate;
	struct zms_ate close_ate;

	*addr += fs->ate_size;
	if ((SECTOR_OFFSET(*addr)) != (fs->sector_size - 2 * fs->ate_size)) {
		return 0;
	}

	/* last ate in sector, do jump to previous sector */
	if (SECTOR_NUM(*addr) == 0U) {
		*addr += ((uint64_t)(fs->sector_count - 1) << ADDR_SECT_SHIFT);
	} else {
		*addr -= (1ULL << ADDR_SECT_SHIFT);
	}

	/* verify if the sector is closed */
	sec_closed = zms_validate_closed_sector(fs, *addr, &empty_ate, &close_ate);
	if (sec_closed < 0) {
		return sec_closed;
	}

	/* Non Closed Sector */
	if (!sec_closed) {
		/* at the end of filesystem */
		*addr = fs->ate_wra;
		return 0;
	}

	/* Update the address here because the header ATEs are valid.*/
	(*addr) &= ADDR_SECT_MASK;
	(*addr) += close_ate.offset;

	return 0;
}

/* walking through allocation entry list, from newest to oldest entries
 * read ate from addr, modify addr to the previous ate
 */
static int zms_prev_ate(struct bm_zms_fs *fs, uint64_t *addr, struct zms_ate *ate)
{
	int rc;

	rc = zms_flash_ate_rd(fs, *addr, ate);
	if (rc) {
		return rc;
	}

	return zms_compute_prev_addr(fs, addr);
}

static void zms_sector_advance(struct bm_zms_fs *fs, uint64_t *addr)
{
	*addr += (1ULL << ADDR_SECT_SHIFT);
	if ((*addr >> ADDR_SECT_SHIFT) == fs->sector_count) {
		*addr -= ((uint64_t)fs->sector_count << ADDR_SECT_SHIFT);
	}
}

/* allocation entry close (this closes the current sector) by writing offset
 * of last ate to the sector end.
 */
static int zms_sector_close(struct bm_zms_fs *fs)
{
	if (cur_op.step == ZMS_OP_WRITE_CLOSE_SECTOR_GARBAGE) {
		/* When we close the sector, we must write all non used ATE with
		 * a non valid (Junk) ATE.
		 * This is needed to avoid some corner cases where some ATEs are
		 * not overwritten and become valid when the cycle counter wrap again
		 * to the same cycle counter of the old ATE.
		 * Example :
		 * - An ATE.cycl_cnt == 0 is written as last ATE of the sector
		 *   - This ATE was never overwritten in the next 255 cycles because of
		 *     large data size
		 *   - Next 256th cycle the leading cycle_cnt is 0, this ATE becomes
		 *     valid even if it is not the case.
		 */
		memset(&cur_op.ate_entry, fs->zms_bm_storage.nvm_info->erase_value,
		       sizeof(struct zms_ate));
		if (SECTOR_OFFSET(fs->ate_wra) && (fs->ate_wra > fs->data_wra)) {
			cur_op.len = sizeof(struct zms_ate);
			cur_op.addr = fs->ate_wra;
			return zms_flash_ate_wrt(fs);
		}
		cur_op.step = ZMS_OP_WRITE_CLOSE_SECTOR_ATE;
	}

	if (cur_op.step == ZMS_OP_WRITE_CLOSE_SECTOR_ATE) {
		cur_op.ate_entry.id = ZMS_HEAD_ID;
		cur_op.ate_entry.len = 0U;
		cur_op.ate_entry.offset = (uint32_t)SECTOR_OFFSET(fs->ate_wra + fs->ate_size);
		cur_op.ate_entry.metadata = 0xffffffff;
		cur_op.ate_entry.cycle_cnt = fs->sector_cycle;
		zms_ate_crc8_update(&cur_op.ate_entry);
		fs->ate_wra = zms_close_ate_addr(fs, fs->ate_wra);
		cur_op.addr = fs->ate_wra;
		cur_op.len = sizeof(struct zms_ate);
		return zms_flash_ate_wrt(fs);
	}

	return 0;
}

static int zms_gc_prepare(struct bm_zms_fs *fs)
{
	int rc;

	zms_sector_advance(fs, &fs->ate_wra);
	rc = zms_get_sector_cycle(fs, fs->ate_wra, &fs->sector_cycle);
	if (rc == -ENOENT) {
		/* sector never used */
		fs->sector_cycle = 0;
	} else if (rc) {
		/* bad flash read */
		return rc;
	}
	fs->data_wra = fs->ate_wra & ADDR_SECT_MASK;
	cur_op.gc.step = ZMS_OP_WRITE_GC_INIT;

	return 0;
}

static uint32_t zms_add_gc_done_ate(struct bm_zms_fs *fs)
{
	LOG_DBG("Adding gc done ate at %llx", fs->ate_wra);
	/* Initialize all members to 0 */
	memset(&cur_op.ate_entry, 0, sizeof(struct zms_ate));
	cur_op.ate_entry.id = ZMS_HEAD_ID;
	cur_op.ate_entry.len = 0U;
	cur_op.ate_entry.offset = (uint32_t)SECTOR_OFFSET(fs->data_wra);
	cur_op.ate_entry.metadata = 0xffffffff;
	cur_op.ate_entry.cycle_cnt = fs->sector_cycle;

	zms_ate_crc8_update(&cur_op.ate_entry);

	cur_op.len = sizeof(struct zms_ate);
	cur_op.addr = fs->ate_wra;
	return zms_flash_ate_wrt(fs);
}

/* This function verifies that the cycle_cnt of the close ATE will not be equal
 * to the cycle_cnt of the empty ATE after incrementing it.
 * This is possible only in these extreme conditions:
 * 1- A Garbage collection operation is interrupted due to a power cut
 * 2- When rebooting, the sector is erased (cycle_cnt incremented)
 * 3- Garbage collection restarts again and got interrupted again by a power cut
 * 4- Steps [2..3] occurred 255 times in a row
 * At this point the sector that we should erase becomes closed.
 */
static inline int zms_verify_and_increment_cycle_cnt(struct bm_zms_fs *fs, uint64_t addr,
						     uint8_t *cycle_cnt)
{
	int rc;
	uint64_t close_addr;
	struct zms_ate close_ate;

	close_addr = zms_close_ate_addr(fs, addr);
	/* Read the second ate in the sector to get the close ATE */
	rc = zms_flash_ate_rd(fs, close_addr, &close_ate);
	if (rc < 0) {
		return rc;
	}

	*cycle_cnt = (*cycle_cnt + 1) % BIT(8);
	/* Verify that the close cycle_cnt is not equal to the incremented value.
	 * If they are equal increment it again.
	 */
	if (close_ate.cycle_cnt == *cycle_cnt) {
		*cycle_cnt = (*cycle_cnt + 1) % BIT(8);
	}

	return 0;
}

static uint32_t zms_add_empty_ate(struct bm_zms_fs *fs, uint64_t addr)
{
	uint8_t cycle_cnt;
	uint32_t rc = 0;

	addr &= ADDR_SECT_MASK;

	cur_op.ate_entry.id = ZMS_HEAD_ID;
	cur_op.ate_entry.len = 0xffff;
	cur_op.ate_entry.offset = 0U;
	cur_op.ate_entry.metadata =
		FIELD_PREP(ZMS_MAGIC_NUMBER_MASK, ZMS_MAGIC_NUMBER) | ZMS_DEFAULT_VERSION;

	rc = zms_get_sector_cycle(fs, addr, &cycle_cnt);
	if (rc == -ENOENT) {
		/* sector never used */
		cycle_cnt = 0;
	} else if (rc) {
		/* bad flash read */
		return rc;
	}

	/* Increase cycle counter */
	rc = zms_verify_and_increment_cycle_cnt(fs, addr, &cycle_cnt);
	if (rc < 0) {
		return rc;
	}
	cur_op.ate_entry.cycle_cnt = cycle_cnt;
	zms_ate_crc8_update(&cur_op.ate_entry);

	cur_op.addr = zms_empty_ate_addr(fs, addr);
	cur_op.len = sizeof(struct zms_ate);
	return zms_flash_ate_wrt(fs);
}

static int zms_get_sector_cycle(struct bm_zms_fs *fs, uint64_t addr, uint8_t *cycle_cnt)
{
	int rc;
	struct zms_ate empty_ate;
	uint64_t empty_addr;

	empty_addr = zms_empty_ate_addr(fs, addr);

	/* read the cycle counter of the current sector */
	rc = zms_flash_ate_rd(fs, empty_addr, &empty_ate);
	if (rc < 0) {
		/* flash error */
		return rc;
	}

	if (zms_empty_ate_valid(fs, &empty_ate)) {
		*cycle_cnt = empty_ate.cycle_cnt;
		return 0;
	}

	/* there is no empty ATE in this sector */
	return -ENOENT;
}

static int zms_get_sector_header(struct bm_zms_fs *fs, uint64_t addr, struct zms_ate *empty_ate,
				 struct zms_ate *close_ate)
{
	int rc;
	uint64_t close_addr;

	close_addr = zms_close_ate_addr(fs, addr);
	/* read the second ate in the sector to get the close ATE */
	rc = zms_flash_ate_rd(fs, close_addr, close_ate);
	if (rc) {
		return rc;
	}

	/* read the first ate in the sector to get the empty ATE */
	rc = zms_flash_ate_rd(fs, close_addr + fs->ate_size, empty_ate);
	if (rc) {
		return rc;
	}

	return 0;
}

/**
 * @brief Helper to find an ATE using its ID
 *
 * @param fs Pointer to file system
 * @param id Id of the entry to be found
 * @param start_addr Address from where the search will start
 * @param end_addr Address where the search will stop
 * @param ate pointer to the found ATE if it exists
 * @param ate_addr Pointer to the address of the found ATE
 *
 * @retval 0 No ATE is found
 * @retval 1 valid ATE with same ID found
 * @retval < 0 An error happened
 */
static int zms_find_ate_with_id(struct bm_zms_fs *fs, uint32_t id, uint64_t start_addr,
				uint64_t end_addr, struct zms_ate *ate, uint64_t *ate_addr)
{
	int rc;
	int previous_sector_num = ZMS_INVALID_SECTOR_NUM;
	uint64_t wlk_prev_addr;
	uint64_t wlk_addr;
	int prev_found = 0;
	struct zms_ate wlk_ate;
	uint8_t current_cycle;

	wlk_addr = start_addr;

	do {
		wlk_prev_addr = wlk_addr;
		rc = zms_prev_ate(fs, &wlk_addr, &wlk_ate);
		if (rc) {
			return rc;
		}

		if (wlk_ate.id == id) {
			/* read the ate cycle only when we change the sector or if it is
			 * the first read ( previous_sector_num == ZMS_INVALID_SECTOR_NUM).
			 */
			rc = zms_get_cycle_on_sector_change(fs, wlk_prev_addr, previous_sector_num,
							    &current_cycle);
			if (rc) {
				return rc;
			}
			if (zms_ate_valid_different_sector(fs, &wlk_ate, current_cycle)) {
				prev_found = 1;
				break;
			}
			previous_sector_num = SECTOR_NUM(wlk_prev_addr);
		}
	} while (wlk_addr != end_addr);

	*ate = wlk_ate;
	*ate_addr = wlk_prev_addr;

	return prev_found;
}

/* garbage collection: the address ate_wra has been updated to the new sector
 * that has just been started. The data to gc is in the sector after this new
 * sector.
 */
static int zms_gc(struct bm_zms_fs *fs)
{
	int rc;
	int sec_closed;
	struct zms_ate close_ate;
	struct zms_ate wlk_ate;
	struct zms_ate empty_ate;
	uint64_t wlk_addr;
	uint64_t wlk_prev_addr;
	uint64_t data_addr;

	if (cur_op.gc.step == ZMS_OP_WRITE_GC_INIT) {
		rc = zms_get_sector_cycle(fs, fs->ate_wra, &fs->sector_cycle);
		if (rc == -ENOENT) {
			/* Erase this new unused sector if needed */
			rc = zms_flash_erase_sector(fs, fs->ate_wra);

			/* sector never used */
			cur_op.gc.step = ZMS_OP_WRITE_GC_INIT_EMPTY_SECTOR;
			return zms_add_empty_ate(fs, fs->ate_wra);
		} else if (rc) {
			/* bad flash read */
			return rc;
		}
		cur_op.gc.previous_cycle = fs->sector_cycle;

		cur_op.gc.sec_addr = (fs->ate_wra & ADDR_SECT_MASK);
		zms_sector_advance(fs, &cur_op.gc.sec_addr);
		cur_op.gc.gc_addr = cur_op.gc.sec_addr + fs->sector_size - fs->ate_size;

		/* verify if the sector is closed */
		sec_closed =
			zms_validate_closed_sector(fs, cur_op.gc.gc_addr, &empty_ate, &close_ate);
		if (sec_closed < 0) {
			return sec_closed;
		}

		/* if the sector is not closed don't do gc */
		if (!sec_closed) {
			cur_op.gc.step = ZMS_OP_WRITE_GC_DONE;
			goto gc_done;
		}

		/* update sector_cycle */
		fs->sector_cycle = empty_ate.cycle_cnt;

		/* stop_addr points to the first ATE before the header ATEs */
		cur_op.gc.stop_addr = cur_op.gc.gc_addr - 2 * fs->ate_size;
		/* At this step empty & close ATEs are valid.
		 * let's start the GC
		 */
		cur_op.gc.gc_addr &= ADDR_SECT_MASK;
		cur_op.gc.gc_addr += close_ate.offset;
		cur_op.gc.step = ZMS_OP_WRITE_GC_EXECUTE;
	}

	if (cur_op.gc.step == ZMS_OP_WRITE_GC_EXECUTE) {
		do {
			cur_op.gc.gc_prev_addr = cur_op.gc.gc_addr;
			rc = zms_prev_ate(fs, &cur_op.gc.gc_addr, &cur_op.ate_entry);
			if (rc) {
				return rc;
			}

			if (!zms_ate_valid(fs, &cur_op.ate_entry) || !cur_op.ate_entry.len) {
				continue;
			}

#ifdef CONFIG_BM_ZMS_LOOKUP_CACHE
			wlk_addr = fs->lookup_cache[zms_lookup_cache_pos(cur_op.ate_entry.id)];

			if (wlk_addr == ZMS_LOOKUP_CACHE_NO_ADDR) {
				wlk_addr = fs->ate_wra;
			}
#else
			wlk_addr = fs->ate_wra;
#endif

			/* Initialize the wlk_prev_addr as if no previous ID will be found */
			wlk_prev_addr = cur_op.gc.gc_prev_addr;
			/* Search for a previous valid ATE with the same ID. If it doesn't exist
			 * then wlk_prev_addr will be equal to gc_prev_addr.
			 */
			rc = zms_find_ate_with_id(fs, cur_op.ate_entry.id, wlk_addr, fs->ate_wra,
						  &wlk_ate, &wlk_prev_addr);
			if (rc < 0) {
				return rc;
			}

			/* if walk_addr has reached the same address as gc_addr, a copy is
			 * needed unless it is a deleted item.
			 */
			if (wlk_prev_addr == cur_op.gc.gc_prev_addr) {
				/* copy needed */
				LOG_DBG("Moving %d, len %d gc_prev_addr %llx from %x to data_wra "
					"%llx ate_wra %llx",
					cur_op.ate_entry.id, cur_op.ate_entry.len,
					cur_op.gc.gc_prev_addr, cur_op.ate_entry.offset,
					fs->data_wra, fs->ate_wra);

				if (cur_op.ate_entry.len > ZMS_DATA_IN_ATE_SIZE) {
					/* Copy Data only when len > 8
					 * Otherwise, Data is already inside ATE
					 */
					data_addr = (cur_op.gc.gc_prev_addr & ADDR_SECT_MASK);
					data_addr += cur_op.ate_entry.offset;
					cur_op.ate_entry.offset =
						(uint32_t)SECTOR_OFFSET(fs->data_wra);
					cur_op.gc.blk_mv_addr = data_addr;
					cur_op.gc.blk_mv_len = cur_op.ate_entry.len;

					return zms_flash_block_move(fs);
				}
				cur_op.gc.step = ZMS_OP_WRITE_GC_ATE_COPY;
				goto ate_copy;
			}
		} while (cur_op.gc.gc_prev_addr != cur_op.gc.stop_addr);
		cur_op.gc.step = ZMS_OP_WRITE_GC_DONE;
	}

ate_copy:
	if (cur_op.gc.step == ZMS_OP_WRITE_GC_ATE_COPY) {
		/* data write (if needed) succeeded, increment data_wra */
		if (cur_op.ate_entry.len > ZMS_DATA_IN_ATE_SIZE) {
			fs->data_wra += zms_al_size(fs, cur_op.ate_entry.len);
		}
		cur_op.ate_entry.cycle_cnt = cur_op.gc.previous_cycle;
		zms_ate_crc8_update(&cur_op.ate_entry);
		cur_op.len = sizeof(struct zms_ate);
		cur_op.addr = fs->ate_wra;
		return zms_flash_ate_wrt(fs);
	}

gc_done:

	if ((cur_op.gc.step == ZMS_OP_WRITE_GC_DONE) ||
	    (cur_op.gc.step == ZMS_OP_WRITE_GC_ATE_COPY_DONE)) {
		/* restore the previous sector_cycle */
		fs->sector_cycle = cur_op.gc.previous_cycle;

		/* Write a GC_done ATE to mark the end of this operation
		 */

		return zms_add_gc_done_ate(fs);
	}

	if (cur_op.gc.step == ZMS_OP_WRITE_GC_DONE_EMPTY_SECTOR) {
		cur_op.gc.gc_count++;
		LOG_DBG("GC done, gc_count %u", cur_op.gc.gc_count);
		/* Erase the GC'ed sector when needed */
		rc = zms_flash_erase_sector(fs, cur_op.gc.sec_addr);

#ifdef CONFIG_BM_ZMS_LOOKUP_CACHE
		zms_lookup_cache_invalidate(fs, cur_op.gc.sec_addr >> ADDR_SECT_SHIFT);
#endif
		return zms_add_empty_ate(fs, cur_op.gc.sec_addr);
	}
	return 0;
}

int bm_zms_clear_execute(void)
{
	int rc = 0;
	struct bm_zms_fs *fs = cur_op.fs;
	uint64_t addr;

	if (cur_op.step == ZMS_OP_CLEAR_START) {
		cur_op.step = ZMS_OP_CLEAR_EXECUTE;
	}

	if (cur_op.step == ZMS_OP_CLEAR_EXECUTE) {
		addr = (uint64_t)cur_op.clear_sector << ADDR_SECT_SHIFT;
		cur_op.clear_sector++;
		rc = zms_flash_erase_sector(fs, addr);
		if (rc) {
			return 0;
		}
		return zms_add_empty_ate(fs, addr);
	}

	return 0;
}

int bm_zms_clear(struct bm_zms_fs *fs)
{
	uint32_t rc = 0;
	zms_op_t cur_clear_op;

	if (!fs->init_flags.initialized) {
		LOG_ERR("zms not initialized");
		return -EACCES;
	}

	NRFX_CRITICAL_SECTION_ENTER();
	memset(&cur_clear_op, 0, sizeof(cur_clear_op));
	cur_clear_op.fs = fs;
	cur_clear_op.op_code = ZMS_OP_CLEAR;
	cur_clear_op.step = ZMS_OP_CLEAR_START;
	cur_clear_op.clear_sector = 0U;
	cur_clear_op.addr = 0U;

	rc = ring_buf_put(&zms_fifo, (uint8_t *)&cur_clear_op, sizeof(zms_op_t));
	NRFX_CRITICAL_SECTION_EXIT();
	if (rc != sizeof(zms_op_t)) {
		return -ENOMEM;
	}

	queue_start();

	return 0;
}

static int zms_init(void)
{
	int rc = 0;
	int ret;
	int sec_closed;
	struct zms_ate last_ate;
	struct zms_ate first_ate;
	struct zms_ate close_ate;
	struct zms_ate empty_ate;
	uint64_t addr = 0U;
	uint32_t i;
	uint32_t closed_sectors = 0;
	bool zms_magic_exist = false;
	struct bm_zms_fs *fs = cur_op.fs;

	if (cur_op.step == ZMS_OP_INIT_START) {
		/* step through the sectors to find a open sector following
		 * a closed sector, this is where bm_zms can write.
		 */
		for (i = 0; i < fs->sector_count; i++) {
			addr = zms_close_ate_addr(fs, ((uint64_t)i << ADDR_SECT_SHIFT));

			/* verify if the sector is closed */
			sec_closed = zms_validate_closed_sector(fs, addr, &empty_ate, &close_ate);
			if (sec_closed < 0) {
				rc = sec_closed;
				goto end;
			}
			/* update cycle count */
			fs->sector_cycle = empty_ate.cycle_cnt;

			if (sec_closed == 1) {
				/* closed sector */
				closed_sectors++;
				/* Let's verify that this is a BM_ZMS storage system */
				if (ZMS_GET_MAGIC_NUMBER(empty_ate.metadata) == ZMS_MAGIC_NUMBER) {
					zms_magic_exist = true;
					/* Let's check that we support this BM_ZMS version */
					if (ZMS_GET_VERSION(empty_ate.metadata) !=
					    ZMS_DEFAULT_VERSION) {
						LOG_ERR("ZMS Version is not supported");
						rc = -ENOTSUP;
						goto end;
					}
				}

				zms_sector_advance(fs, &addr);
				/* addr is pointing to the close ATE */
				/* verify if the sector is Open */
				sec_closed = zms_validate_closed_sector(fs, addr, &empty_ate,
									&close_ate);
				if (sec_closed < 0) {
					rc = sec_closed;
					goto end;
				}
				/* update cycle count */
				fs->sector_cycle = empty_ate.cycle_cnt;

				if (!sec_closed) {
					/* We found an Open sector following a closed one */
					break;
				}
			}
		}
		/* all sectors are closed, and bm_zms magic number not found. This is not a bm_zms
		 * fs.
		 */
		if ((closed_sectors == fs->sector_count) && !zms_magic_exist) {
			rc = -ENOEXEC;
			goto end;
		}
		/* TODO: add a recovery mechanism here if the BM_ZMS magic number exist but all
		 * sectors are closed
		 */

		if (i == fs->sector_count) {
			/* none of the sectors were closed, which means that the first
			 * sector is the one in use, except if there are only 2 sectors.
			 * Let's check if the last sector has valid ATEs otherwise set
			 * the open sector to the first one.
			 */
			rc = zms_flash_ate_rd(fs, addr - fs->ate_size, &first_ate);
			if (rc) {
				goto end;
			}
			if (!zms_ate_valid(fs, &first_ate)) {
				zms_sector_advance(fs, &addr);
			}
			rc = zms_get_sector_header(fs, addr, &empty_ate, &close_ate);
			if (rc) {
				goto end;
			}

			if (zms_empty_ate_valid(fs, &empty_ate)) {
				/* Empty ATE is valid, let's verify that this is a BM_ZMS storage
				 * system
				 */
				if (ZMS_GET_MAGIC_NUMBER(empty_ate.metadata) == ZMS_MAGIC_NUMBER) {
					zms_magic_exist = true;
					/* Let's check the version */
					if (ZMS_GET_VERSION(empty_ate.metadata) !=
					    ZMS_DEFAULT_VERSION) {
						LOG_ERR("ZMS Version is not supported");
						rc = -ENOTSUP;
						goto end;
					}
				}
				fs->sector_cycle = empty_ate.cycle_cnt;
			} else {
				cur_op.step = ZMS_OP_INIT_ALL_OPEN_ADD_EMPTY_ATE;
				rc = zms_flash_erase_sector(fs, addr);
				if (rc) {
					goto end;
				}
				cur_op.init.addr = addr;
				return zms_add_empty_ate(fs, addr);
			}
		}
		cur_op.step = ZMS_OP_INIT_RECOVER_LAST_ATE;
		cur_op.init.addr = addr;
	}

	if (cur_op.step == ZMS_OP_INIT_ALL_OPEN_ADD_EMPTY_ATE) {
		rc = zms_get_sector_cycle(fs, cur_op.init.addr, &fs->sector_cycle);
		if (rc == -ENOENT) {
			/* sector never used */
			fs->sector_cycle = 0;
		} else if (rc) {
			/* bad flash read */
			goto end;
		}
		cur_op.step = ZMS_OP_INIT_RECOVER_LAST_ATE;
	}

	if (cur_op.step == ZMS_OP_INIT_RECOVER_LAST_ATE) {
		/* addr contains address of closing ate in the most recent sector,
		 * search for the last valid ate using the recover_last_ate routine
		 * and also update the data_wra
		 */
		uint64_t ate_wra = cur_op.init.addr, data_wra = cur_op.init.data_wra;

		rc = zms_recover_last_ate(fs, &ate_wra, &data_wra);
		cur_op.init.addr = ate_wra;
		cur_op.init.data_wra = data_wra;
		if (rc) {
			goto end;
		}

		/* addr contains address of the last valid ate in the most recent sector
		 * data_wra contains the data write address of the current sector
		 */
		fs->ate_wra = cur_op.init.addr;
		fs->data_wra = cur_op.init.data_wra;

		/* fs->ate_wra should point to the next available entry. This is normally
		 * the next position after the one found by the recovery function.
		 * Let's verify that it doesn't contain any valid ATE, otherwise search for
		 * an empty position
		 */
		while (fs->ate_wra >= fs->data_wra) {
			rc = zms_flash_ate_rd(fs, fs->ate_wra, &last_ate);
			if (rc) {
				goto end;
			}
			if (!zms_ate_valid(fs, &last_ate)) {
				/* found empty location */
				break;
			}

			/* ate on the last position within the sector is
			 * reserved for deletion an entry
			 */
			if ((fs->ate_wra == fs->data_wra) && last_ate.len) {
				/* not a delete ate */
				rc = -EFAULT;
				goto end;
			}

			fs->ate_wra -= fs->ate_size;
		}

		/* The sector after the write sector is either empty with a valid empty ATE (regular
		 * case) or it has never been used or it is a closed sector (GC didn't finish) If it
		 * is a closed sector we must look for a valid GC done ATE in the current write
		 * sector, if it is missing, we need to restart gc because it has been interrupted.
		 * If no valid empty ATE is found then it has never been used. Just erase it by
		 * adding a valid empty ATE. When gc needs to be restarted, first erase the sector
		 * by adding an empty ATE otherwise the data might not fit into the sector.
		 */
		addr = zms_close_ate_addr(fs, fs->ate_wra);
		zms_sector_advance(fs, &addr);

		/* verify if the sector is closed */
		sec_closed = zms_validate_closed_sector(fs, addr, &empty_ate, &close_ate);
		if (sec_closed < 0) {
			rc = sec_closed;
			goto end;
		}

		if (sec_closed == 1) {
			/* The sector after fs->ate_wrt is closed.
			 * Look for a marker (gc_done_ate) that indicates that gc was finished.
			 */
			bool gc_done_marker = false;
			struct zms_ate gc_done_ate;

			fs->sector_cycle = empty_ate.cycle_cnt;
			addr = fs->ate_wra + fs->ate_size;
			while (SECTOR_OFFSET(addr) < (fs->sector_size - 2 * fs->ate_size)) {
				rc = zms_flash_ate_rd(fs, addr, &gc_done_ate);
				if (rc) {
					goto end;
				}

				if (zms_gc_done_ate_valid(fs, &gc_done_ate)) {
					gc_done_marker = true;
					break;
				}
				addr += fs->ate_size;
			}

			if (gc_done_marker) {
				/* erase the next sector */
				LOG_DBG("GC Done marker found");
				addr = fs->ate_wra & ADDR_SECT_MASK;
				zms_sector_advance(fs, &addr);
				ret = zms_flash_erase_sector(fs, addr);
				if (ret) {
					rc = -EIO;
					goto end;
				}
				cur_op.step = ZMS_OP_INIT_ADD_EMPTY_ATE_GC_DONE;
				return zms_add_empty_ate(fs, addr);
			}
			LOG_DBG("No GC Done marker found: restarting gc");

			/* Let's point to the first writable position */
			fs->ate_wra &= ADDR_SECT_MASK;
			fs->ate_wra += (fs->sector_size - 3 * fs->ate_size);
			fs->data_wra = (fs->ate_wra & ADDR_SECT_MASK);
#ifdef CONFIG_BM_ZMS_LOOKUP_CACHE
			/**
			 * At this point, the lookup cache wasn't built but the gc function need to
			 *use it. So, temporarily, we set the lookup cache to the end of the fs. The
			 *cache will be rebuilt afterwards
			 **/
			for (i = 0; i < CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE; i++) {
				fs->lookup_cache[i] = fs->ate_wra;
			}
#endif

			rc = zms_flash_erase_sector(fs, fs->ate_wra);
			if (rc) {
				goto end;
			}
			cur_op.step = ZMS_OP_INIT_ADD_EMPTY_ATE_GC_TODO;
			return zms_add_empty_ate(fs, fs->ate_wra);
		}
		cur_op.step = ZMS_OP_INIT_ADD_GC_DONE;
	}
	if (cur_op.step == ZMS_OP_INIT_GC_START) {
		rc = zms_gc_prepare(fs);
		if (rc) {
			goto end;
		}
		return zms_gc(fs);
	}
	if (cur_op.step == ZMS_OP_INIT_GC) {
		return zms_gc(fs);
	}

	if (cur_op.step == ZMS_OP_INIT_ADD_GC_DONE) {
end:
#ifdef CONFIG_BM_ZMS_LOOKUP_CACHE
		if (!rc) {
			rc = zms_lookup_cache_rebuild(fs);
		}
#endif
		/* If the sector is empty add a gc done ate to avoid having insufficient
		 * space when doing gc.
		 */
		if ((!rc) && (SECTOR_OFFSET(fs->ate_wra) == (fs->sector_size - 3 * fs->ate_size))) {
			return zms_add_gc_done_ate(fs);
		}
		cur_op.step = ZMS_OP_INIT_DONE;
	}

	if (cur_op.step == ZMS_OP_INIT_DONE) {
		fs->init_flags.initialized = true;
		fs->init_flags.initializing = false;
		cur_op.op_completed = true;
		rc = 0;
	}
	return rc;
}

int bm_zms_mount(struct bm_zms_fs *fs)
{
	int rc;
	size_t write_block_size;
	zms_op_t cur_init_op;

	/* Initialize BM Storage */
	fs->zms_bm_storage.start_addr = fs->offset;
	fs->zms_bm_storage.end_addr = fs->offset + fs->sector_size * fs->sector_count;
	fs->zms_bm_storage.evt_handler = zms_event_handler;
	rc = bm_storage_init(&fs->zms_bm_storage);
	if (rc) {
		LOG_ERR("bm_storage_init() failed, rc %u", rc);
		return -EIO;
	}

	fs->ate_size = zms_al_size(fs, sizeof(struct zms_ate));
	write_block_size = fs->zms_bm_storage.nvm_info->program_unit;

	/* check that the write block size is supported */
	if (write_block_size > ZMS_BLOCK_SIZE || write_block_size == 0) {
		LOG_ERR("Unsupported write block size");
		return -EINVAL;
	}

	/* When the device need erase operations before write let's check that
	 * sector size is a multiple of pagesize
	 */
	if (!fs->zms_bm_storage.nvm_info->no_explicit_erase) {
		if (!fs->sector_size ||
		    fs->sector_size % fs->zms_bm_storage.nvm_info->erase_unit) {
			LOG_ERR("Invalid sector size");
			return -EINVAL;
		}
	}

	/* we need at least 5 aligned ATEs size as the minimum sector size
	 * 1 close ATE, 1 empty ATE, 1 GC done ATE, 1 Delete ATE, 1 ID/Value ATE
	 */
	if (fs->sector_size < ZMS_MIN_ATE_NUM * fs->ate_size) {
		LOG_ERR("Invalid sector size, should be at least %zu",
			ZMS_MIN_ATE_NUM * fs->ate_size);
		return -EINVAL;
	}

	/* check the number of sectors, it should be at least 2 */
	if (fs->sector_count < 2) {
		LOG_ERR("Configuration error - sector count below minimum requirement (2)");
		return -EINVAL;
	}

	if (fs->init_flags.initializing) {
		LOG_ERR("zms already initializing");
		return -EBUSY;
	}
	fs->init_flags.initializing = true;
	fs->init_flags.initialized = false;
	fs->ongoing_writes = ATOMIC_INIT(0);

	NRFX_CRITICAL_SECTION_ENTER();
	memset(&cur_init_op, 0, sizeof(cur_init_op));
	cur_init_op.fs = fs;
	cur_init_op.op_code = ZMS_OP_INIT;
	cur_init_op.step = ZMS_OP_INIT_START;

	rc = ring_buf_put(&zms_fifo, (uint8_t *)&cur_init_op, sizeof(zms_op_t));
	NRFX_CRITICAL_SECTION_EXIT();
	if (rc != sizeof(zms_op_t)) {
		return -ENOMEM;
	}

	queue_start();

	return 0;
}

static void zms_verify_space(zms_op_t *op)
{
	struct bm_zms_fs *fs = op->fs;

	/* We need to make sure that we leave the ATE at address 0x0 of the sector
	 * empty (even for delete ATE). Otherwise, the fs->ate_wra will be decremented
	 * after this write by ate_size and it will underflow.
	 * So the first position of a sector (fs->ate_wra = 0x0) is forbidden for ATEs
	 * and the second position could be written only be a delete ATE.
	 */
	if ((SECTOR_OFFSET(fs->ate_wra)) && (fs->ate_wra >= (fs->data_wra + op->required_space)) &&
	    (SECTOR_OFFSET(fs->ate_wra - fs->ate_size) || !op->data_len)) {
		op->step = ZMS_OP_WRITE_EXECUTE;
		if (op->data_len > ZMS_DATA_IN_ATE_SIZE) {
			op->sub_step = ZMS_OP_WRITE_SUB_STEP_DATA1;
		} else {
			op->sub_step = ZMS_OP_WRITE_SUB_STEP_ATE1;
		}
		op->gc.gc_count = 0;
	} else {
		if (SECTOR_OFFSET(fs->ate_wra) && (fs->ate_wra > fs->data_wra)) {
			op->step = ZMS_OP_WRITE_CLOSE_SECTOR_GARBAGE;
		} else {
			op->step = ZMS_OP_WRITE_CLOSE_SECTOR_ATE;
		}
	}
}

ssize_t bm_zms_write(struct bm_zms_fs *fs, uint32_t id, const void *data, size_t len)
{
	size_t data_size;
	uint32_t required_space = 0U; /* no space, appropriate for delete ate */
	zms_op_t cur_write_op;
	uint32_t rc = 0;

	if (!fs->init_flags.initialized) {
		LOG_ERR("zms not initialized");
		return -EACCES;
	}

	LOG_DBG("%s id %u, len %zu, ate_wra 0x%llx, data_wra 0x%llx", __func__, id, len,
		fs->ate_wra, fs->data_wra);
	data_size = zms_al_size(fs, len);

	/* The maximum data size is sector size - 5 ate
	 * where: 1 ate for data, 1 ate for sector close, 1 ate for empty,
	 * 1 ate for gc done, and 1 ate to always allow a delete.
	 * We cannot also store more than 64 KB of data
	 */
	if ((len > (fs->sector_size - 5 * fs->ate_size)) || (len > UINT16_MAX) ||
	    ((len > 0) && (data == NULL))) {
		return -EINVAL;
	}

	/* calculate required space if the entry contains data */
	if (data_size) {
		/* Leave space for delete ate */
		if (len > ZMS_DATA_IN_ATE_SIZE) {
			required_space = data_size + fs->ate_size;
		} else {
			required_space = fs->ate_size;
		}
	}

	NRFX_CRITICAL_SECTION_ENTER();
	memset(&cur_write_op, 0, sizeof(cur_write_op));
	cur_write_op.fs = fs;
	cur_write_op.op_code = ZMS_OP_WRITE;
	cur_write_op.step = ZMS_OP_WRITE_STARTUP;
	cur_write_op.len = len;
	cur_write_op.data_len = len;
	cur_write_op.data = data;
	cur_write_op.app_data = data;
	cur_write_op.id = id;
	cur_write_op.required_space = required_space;

	rc = ring_buf_put(&zms_fifo, (uint8_t *)&cur_write_op, sizeof(zms_op_t));
	NRFX_CRITICAL_SECTION_EXIT();
	if (rc != sizeof(zms_op_t)) {
		return -ENOMEM;
	}

	atomic_add(&fs->ongoing_writes, 1);
	queue_start();

	return len;
}

static int zms_write_execute(void)
{
	int rc = 0;
	struct bm_zms_fs *fs = cur_op.fs;

	if (cur_op.gc.gc_count == (fs->sector_count - 1)) {
		/* gc'ed all sectors, no extra space will be created
		 * by extra gc.
		 */
		rc = -ENOSPC;
		LOG_ERR("No space in flash, gc_count %u, sector_count %u", cur_op.gc.gc_count,
			fs->sector_count);
		goto end;
	}

	switch (cur_op.step) {
	case ZMS_OP_WRITE_EXECUTE:
		return zms_flash_write_entry(fs);
	case ZMS_OP_WRITE_CLOSE_SECTOR_GARBAGE:
	case ZMS_OP_WRITE_CLOSE_SECTOR_ATE:
		return zms_sector_close(fs);
	case ZMS_OP_WRITE_CLOSE_SECTOR_DONE:
		rc = zms_gc_prepare(fs);
		if (rc) {
			goto end;
		}
		cur_op.step = ZMS_OP_WRITE_GC;
		return zms_gc(fs);
	case ZMS_OP_WRITE_GC:
		return zms_gc(fs);
	case ZMS_OP_WRITE_DONE:
		if (cur_op.data_len > ZMS_DATA_IN_ATE_SIZE) {
			fs->data_wra += zms_al_size(fs, cur_op.data_len);
		}
		cur_op.op_completed = true;
		return 0;
	default:
		LOG_ERR("Unknown step %d", cur_op.step);
		rc = -EIO;
		goto end;
	}
	rc = 0;
end:
	return rc;
}

int bm_zms_delete(struct bm_zms_fs *fs, uint32_t id)
{
	return bm_zms_write(fs, id, NULL, 0);
}

ssize_t bm_zms_read_hist(struct bm_zms_fs *fs, uint32_t id, void *data, size_t len, uint32_t cnt)
{
	int rc;
	int prev_found = 0;
	uint64_t wlk_addr;
	uint64_t rd_addr = 0;
	uint64_t wlk_prev_addr = 0;
	uint32_t cnt_his;
	struct zms_ate wlk_ate;
#ifdef CONFIG_BM_ZMS_DATA_CRC
	uint32_t computed_data_crc;
#endif

	if (!fs->init_flags.initialized) {
		LOG_ERR("zms not initialized");
		return -EACCES;
	}

	cnt_his = 0U;

#ifdef CONFIG_BM_ZMS_LOOKUP_CACHE
	wlk_addr = fs->lookup_cache[zms_lookup_cache_pos(id)];

	if (wlk_addr == ZMS_LOOKUP_CACHE_NO_ADDR) {
		rc = -ENOENT;
		goto err;
	}
#else
	wlk_addr = fs->ate_wra;
#endif

	while (cnt_his <= cnt) {
		wlk_prev_addr = wlk_addr;
		/* Search for a previous valid ATE with the same ID */
		prev_found = zms_find_ate_with_id(fs, id, wlk_addr, fs->ate_wra, &wlk_ate,
						  &wlk_prev_addr);
		if (prev_found < 0) {
			return prev_found;
		}
		if (prev_found) {
			cnt_his++;
			/* wlk_prev_addr contain the ATE address of the previous found ATE. */
			rd_addr = wlk_prev_addr;
			/*
			 * compute the previous ATE address in case we need to start
			 * the research again.
			 */
			rc = zms_compute_prev_addr(fs, &wlk_prev_addr);
			if (rc) {
				return rc;
			}
			/* wlk_addr will be the start research address in the next loop */
			wlk_addr = wlk_prev_addr;
		} else {
			break;
		}
	}

	if (((!prev_found) || (wlk_ate.id != id)) || (wlk_ate.len == 0U) || (cnt_his < cnt)) {
		return -ENOENT;
	}

	if (wlk_ate.len <= ZMS_DATA_IN_ATE_SIZE) {
		/* data is stored in the ATE */
		if (data) {
			memcpy(data, &wlk_ate.data, MIN(len, wlk_ate.len));
		}
	} else {
		rd_addr &= ADDR_SECT_MASK;
		rd_addr += wlk_ate.offset;
		/* do not read or copy data if pointer is NULL */
		if (data) {
			rc = zms_flash_rd(fs, rd_addr, data, MIN(len, wlk_ate.len));
			if (rc) {
				goto err;
			}
		}
#ifdef CONFIG_BM_ZMS_DATA_CRC
		/* Do not compute CRC for partial reads as CRC won't match */
		if (len >= wlk_ate.len) {
			computed_data_crc = crc32_ieee(data, wlk_ate.len);
			if (computed_data_crc != wlk_ate.data_crc) {
				LOG_ERR("Invalid data CRC: ATE_CRC=0x%08X, "
					"computed_data_crc=0x%08X",
					wlk_ate.data_crc, computed_data_crc);
				return -EIO;
			}
		}
#endif
	}

	return wlk_ate.len;

err:
	return rc;
}

ssize_t bm_zms_read(struct bm_zms_fs *fs, uint32_t id, void *data, size_t len)
{
	int rc;

	rc = bm_zms_read_hist(fs, id, data, len, 0);
	if (rc < 0) {
		return rc;
	}

	/* returns the minimum between ATE data length and requested length */
	return MIN(rc, len);
}

ssize_t bm_zms_get_data_length(struct bm_zms_fs *fs, uint32_t id)
{
	int rc;

	rc = bm_zms_read_hist(fs, id, NULL, 0, 0);

	return rc;
}

ssize_t bm_zms_calc_free_space(struct bm_zms_fs *fs)
{
	int rc;
	int previous_sector_num = ZMS_INVALID_SECTOR_NUM;
	int prev_found = 0;
	int sec_closed;
	struct zms_ate step_ate;
	struct zms_ate wlk_ate;
	struct zms_ate empty_ate;
	struct zms_ate close_ate;
	uint64_t step_addr;
	uint64_t wlk_addr;
	uint64_t step_prev_addr;
	uint64_t wlk_prev_addr;
	uint64_t data_wra = 0U;
	uint8_t current_cycle;
	ssize_t free_space = 0;
	const uint32_t second_to_last_offset = (2 * fs->ate_size);

	if (!fs->init_flags.initialized) {
		LOG_ERR("zms not initialized");
		return -EACCES;
	}

	/*
	 * There is always a closing ATE , an empty ATE, a GC_done ATE and a reserved ATE for
	 * deletion in each sector.
	 * And there is always one reserved Sector for garbage collection operations
	 */
	free_space = (fs->sector_count - 1) * (fs->sector_size - 4 * fs->ate_size);

	step_addr = fs->ate_wra;

	do {
		step_prev_addr = step_addr;
		rc = zms_prev_ate(fs, &step_addr, &step_ate);
		if (rc) {
			return rc;
		}

		/* When changing the sector let's get the new cycle counter */
		rc = zms_get_cycle_on_sector_change(fs, step_prev_addr, previous_sector_num,
						    &current_cycle);
		if (rc) {
			return rc;
		}
		previous_sector_num = SECTOR_NUM(step_prev_addr);

		/* Invalid and deleted ATEs are free spaces.
		 * Header ATEs are already retrieved from free space
		 */
		if (!zms_ate_valid_different_sector(fs, &step_ate, current_cycle) ||
		    (step_ate.id == ZMS_HEAD_ID) || (step_ate.len == 0)) {
			continue;
		}

		wlk_addr = step_addr;
		/* Try to find if there is a previous valid ATE with same ID */
		prev_found = zms_find_ate_with_id(fs, step_ate.id, wlk_addr, step_addr, &wlk_ate,
						  &wlk_prev_addr);
		if (prev_found < 0) {
			return prev_found;
		}

		/* If no previous ATE is found, then this is a valid ATE that cannot be
		 * Garbage Collected
		 */
		if (!prev_found || (wlk_prev_addr == step_prev_addr)) {
			if (step_ate.len > ZMS_DATA_IN_ATE_SIZE) {
				free_space -= zms_al_size(fs, step_ate.len);
			}
			free_space -= fs->ate_size;
		}
	} while (step_addr != fs->ate_wra);

	/* we must keep the sector_cycle before we start looking into special cases */
	current_cycle = fs->sector_cycle;

	/* Let's look now for special cases where some sectors have only ATEs with
	 * small data size.
	 */

	for (int i = 0; i < fs->sector_count; i++) {
		step_addr = zms_close_ate_addr(fs, ((uint64_t)i << ADDR_SECT_SHIFT));

		/* verify if the sector is closed */
		sec_closed = zms_validate_closed_sector(fs, step_addr, &empty_ate, &close_ate);
		if (sec_closed < 0) {
			return sec_closed;
		}

		/* If the sector is closed and its offset is pointing to a position less than the
		 * 3rd to last ATE position in a sector, it means that we need to leave the second
		 * to last ATE empty.
		 */
		if ((sec_closed == 1) && (close_ate.offset <= second_to_last_offset)) {
			free_space -= fs->ate_size;
		} else if (!sec_closed) {
			/* sector is open, let's recover the last ATE */
			fs->sector_cycle = empty_ate.cycle_cnt;
			rc = zms_recover_last_ate(fs, &step_addr, &data_wra);
			if (rc) {
				return rc;
			}
			if (SECTOR_OFFSET(step_addr) <= second_to_last_offset) {
				free_space -= fs->ate_size;
			}
		}
	}
	/* restore sector cycle */
	fs->sector_cycle = current_cycle;

	return free_space;
}

ssize_t bm_zms_active_sector_free_space(struct bm_zms_fs *fs)
{
	if (!fs->init_flags.initialized) {
		LOG_ERR("ZMS not initialized");
		return -EACCES;
	}

	return fs->ate_wra - fs->data_wra - fs->ate_size;
}
