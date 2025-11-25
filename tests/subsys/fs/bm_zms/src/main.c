/*
 * Copyright (c) 2024 BayLibre SAS
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/crc.h>
#include <zephyr/ztest.h>

#if defined(CONFIG_BOARD_NATIVE_SIM)
#include <zephyr/kernel.h>
#endif

#include <bm/fs/bm_zms.h>
#include <bm_zms_priv.h>

/* Arbitrary block size. */
#define TEST_SECTOR_SIZE 1024U
#define TEST_SECTOR_COUNT 4U
#define TEST_PARTITION_SIZE (TEST_SECTOR_SIZE * TEST_SECTOR_COUNT)
#define TEST_DATA_ID 1

#if defined(CONFIG_BOARD_NATIVE_SIM)
static uint8_t mem[TEST_PARTITION_SIZE];
#define TEST_PARTITION_START ((off_t)mem)
K_SEM_DEFINE(mount_sem, 0, 1);
K_SEM_DEFINE(clear_sem, 0, 1);
K_SEM_DEFINE(write_sem, 0, 1);
#else
#define STORAGE_NODE DT_NODELABEL(storage_partition)
#define TEST_PARTITION_START DT_REG_ADDR(STORAGE_NODE)
#endif

#if defined(CONFIG_SOFTDEVICE)
static volatile bool write_notif;
#endif

struct bm_zms_fixture {
	struct bm_zms_fs fs;
	struct bm_zms_fs_config config;
};

static bool nvm_is_full;
static bool fs_is_init;

static void wait_for_write(void)
{
#if defined(CONFIG_BOARD_NATIVE_SIM)
	k_sem_take(&write_sem, K_FOREVER);
#elif defined(CONFIG_SOFTDEVICE)
	while (!write_notif) {
		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
	}
	write_notif = false;
#endif
}

static void wait_for_mount(void)
{
#if defined(CONFIG_BOARD_NATIVE_SIM)
	k_sem_take(&mount_sem, K_FOREVER);
#elif defined(CONFIG_SOFTDEVICE)
	while (!is_init) {
		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
	}
#endif
}

static void wait_for_clear(void)
{
#if defined(CONFIG_BOARD_NATIVE_SIM)
	k_sem_take(&clear_sem, K_FOREVER);
#elif defined(CONFIG_SOFTDEVICE)
	while (fs_is_init) {
		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
	}
#endif
}

void bm_zms_test_evt_handler(struct bm_zms_evt const *evt)
{
	switch (evt->evt_type) {
	case BM_ZMS_EVT_MOUNT:
		fs_is_init = true;
#if defined(CONFIG_BOARD_NATIVE_SIM)
		k_sem_give(&mount_sem);
#endif
		zassert_true(evt->result == 0, "bm_zms_mount call failure: %d",
			     evt->result);
		break;
	case BM_ZMS_EVT_CLEAR:
		fs_is_init = false;
#if defined(CONFIG_BOARD_NATIVE_SIM)
		k_sem_give(&clear_sem);
#endif
		zassert_true(evt->result == 0, "bm_zms_clear call failure: %d",
			     evt->result);
		break;
	case BM_ZMS_EVT_WRITE:
	case BM_ZMS_EVT_DELETE:
#if defined(CONFIG_BOARD_NATIVE_SIM)
		k_sem_give(&write_sem);
#elif defined(CONFIG_SOFTDEVICE)
		write_notif = true;
#endif
		if (evt->result == -ENOSPC) {
			nvm_is_full = true;
			return;
		} else if (evt->result) {
			printf("BM_ZMS Error received %d\n", evt->result);
		}

		break;
	default:
		printf("BM_ZMS unexpected event received %d\n", evt->evt_type);
		break;
	}
}

static void *setup(void)
{
	static struct bm_zms_fixture fixture;

	memset(&fixture, 0, sizeof(struct bm_zms_fixture));
	fixture.config.offset = TEST_PARTITION_START;
	fixture.config.sector_size = TEST_SECTOR_SIZE;
	fixture.config.sector_count = TEST_SECTOR_COUNT;
	fixture.config.evt_handler = bm_zms_test_evt_handler;

	return &fixture;
}

static void before(void *data)
{
	nvm_is_full = false;
}

static void after(void *data)
{
	struct bm_zms_fixture *fixture = (struct bm_zms_fixture *)data;

	/* Clear BM_ZMS */
	if (fs_is_init) {
		int err;

		err = bm_zms_clear(&fixture->fs);
		zassert_true(err == 0, "zms_clear call failure: %d", err);
		wait_for_clear();
	}

	fixture->config.sector_count = TEST_SECTOR_COUNT;
}

ZTEST_SUITE(bm_zms, NULL, setup, before, after, NULL);

ZTEST_F(bm_zms, test_bm_zms_mount)
{
	int err;

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");
}

static void execute_long_pattern_write(uint32_t id, struct bm_zms_fs *fs)
{
	char rd_buf[512];
	char wr_buf[512];
	char pattern[] = {0xDE, 0xAD, 0xBE, 0xEF};
	size_t len;

	len = bm_zms_read(fs, id, rd_buf, sizeof(rd_buf));
	zassert_true(len == -ENOENT, "bm_zms_read unexpected failure");

	BUILD_ASSERT((sizeof(wr_buf) % sizeof(pattern)) == 0);
	for (int i = 0; i < sizeof(wr_buf); i += sizeof(pattern)) {
		memcpy(wr_buf + i, pattern, sizeof(pattern));
	}

	len = bm_zms_write(fs, id, wr_buf, sizeof(wr_buf));
	zassert_true(len == sizeof(wr_buf), "bm_zms_write failed");

	wait_for_write();

	len = bm_zms_read(fs, id, rd_buf, sizeof(rd_buf));
	zassert_true(len == sizeof(rd_buf), "bm_zms_read unexpected failure");
	zassert_mem_equal(wr_buf, rd_buf, sizeof(rd_buf),
			  "RD buff should be equal to the WR buff");
}

ZTEST_F(bm_zms, test_bm_zms_write)
{
	int err;

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	zassert_true(err == 0, "zms_mount call failure: %d", err);
	wait_for_mount();

	execute_long_pattern_write(TEST_DATA_ID, &fixture->fs);
}

ZTEST_F(bm_zms, test_zms_gc)
{
	int err;
	int len;
	uint8_t buf[32];
	uint8_t rd_buf[32];
	const uint8_t max_id = 10;
	/* 21st write will trigger GC. */
	const uint16_t max_writes = 21;

	fixture->config.sector_count = 2;

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "zms_mount call failure: %d", err);

	for (int i = 0; i < max_writes; i++) {
		uint8_t id = (i % max_id);
		uint8_t id_data = id + max_id * (i / max_id);

		memset(buf, id_data, sizeof(buf));

		len = bm_zms_write(&fixture->fs, id, buf, sizeof(buf));
		wait_for_write();
		zassert_true(len == sizeof(buf), "bm_zms_write failed");
	}

	for (int id = 0; id < max_id; id++) {
		len = bm_zms_read(&fixture->fs, id, rd_buf, sizeof(buf));
		zassert_true(len == sizeof(rd_buf), "bm_zms_read unexpected failure");

		for (int i = 0; i < sizeof(rd_buf); i++) {
			rd_buf[i] = rd_buf[i] % max_id;
			buf[i] = id;
		}
		zassert_mem_equal(buf, rd_buf, sizeof(rd_buf),
				  "RD buff should be equal to the WR buff");
	}

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	for (int id = 0; id < max_id; id++) {
		len = bm_zms_read(&fixture->fs, id, rd_buf, sizeof(buf));
		zassert_true(len == sizeof(rd_buf), "bm_zms_read unexpected failure %d", len);

		for (int i = 0; i < sizeof(rd_buf); i++) {
			rd_buf[i] = rd_buf[i] % max_id;
			buf[i] = id;
		}
		zassert_mem_equal(buf, rd_buf, sizeof(rd_buf),
				  "RD buff should be equal to the WR buff");
	}
}

static void write_content(uint32_t max_id, uint32_t begin, uint32_t end, struct bm_zms_fs *fs)
{
	uint8_t buf[32];
	ssize_t len;

	for (int i = begin; i < end; i++) {
		uint8_t id = (i % max_id);
		uint8_t id_data = id + max_id * (i / max_id);

		memset(buf, id_data, sizeof(buf));

		len = bm_zms_write(fs, id, buf, sizeof(buf));
		wait_for_write();
		zassert_true(len == sizeof(buf), "bm_zms_write failed");
	}
}

static void check_content(uint32_t max_id, struct bm_zms_fs *fs)
{
	uint8_t rd_buf[32];
	uint8_t buf[32];
	ssize_t len;

	for (int id = 0; id < max_id; id++) {
		len = bm_zms_read(fs, id, rd_buf, sizeof(buf));
		zassert_true(len == sizeof(rd_buf), "bm_zms_read unexpected failure");

		for (int i = 0; i < ARRAY_SIZE(rd_buf); i++) {
			rd_buf[i] = rd_buf[i] % max_id;
			buf[i] = id;
		}
		zassert_mem_equal(buf, rd_buf, sizeof(rd_buf),
				  "RD buff should be equal to the WR buff");
	}
}

/**
 * Full round of GC over 3 sectors
 */
ZTEST_F(bm_zms, test_zms_gc_3sectors)
{
	int err;
	const uint16_t max_id = 10;
	/* 41st write will trigger 1st GC. */
	const uint16_t max_writes = 41;
	/* 61st write will trigger 2nd GC. */
	const uint16_t max_writes_2 = 41 + 20;
	/* 81st write will trigger 3rd GC. */
	const uint16_t max_writes_3 = 41 + 20 + 20;
	/* 101st write will trigger 4th GC. */
	const uint16_t max_writes_4 = 41 + 20 + 20 + 20;

	fixture->config.sector_count = 3;

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure err %d", err);
	zassert_equal(fixture->fs.ate_wra >> ADDR_SECT_SHIFT, 0, "unexpected write sector");

	/* Trigger 1st GC */
	write_content(max_id, 0, max_writes, &fixture->fs);

	/* sector sequence: empty,closed, write */
	zassert_equal(fixture->fs.ate_wra >> ADDR_SECT_SHIFT, 2, "unexpected write sector");
	check_content(max_id, &fixture->fs);

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	zassert_equal(fixture->fs.ate_wra >> ADDR_SECT_SHIFT, 2, "unexpected write sector");
	check_content(max_id, &fixture->fs);

	/* Trigger 2nd GC */
	write_content(max_id, max_writes, max_writes_2, &fixture->fs);

	/* sector sequence: write, empty, closed */
	zassert_equal(fixture->fs.ate_wra >> ADDR_SECT_SHIFT, 0, "unexpected write sector");
	check_content(max_id, &fixture->fs);

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	zassert_true(err == 0, "bm_zms_mount call failure");
	wait_for_mount();

	zassert_equal(fixture->fs.ate_wra >> ADDR_SECT_SHIFT, 0, "unexpected write sector");
	check_content(max_id, &fixture->fs);

	/* Trigger 3rd GC */
	write_content(max_id, max_writes_2, max_writes_3, &fixture->fs);

	/* sector sequence: closed, write, empty */
	zassert_equal(fixture->fs.ate_wra >> ADDR_SECT_SHIFT, 1, "unexpected write sector");
	check_content(max_id, &fixture->fs);

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	zassert_equal(fixture->fs.ate_wra >> ADDR_SECT_SHIFT, 1, "unexpected write sector");
	check_content(max_id, &fixture->fs);

	/* Trigger 4th GC */
	write_content(max_id, max_writes_3, max_writes_4, &fixture->fs);

	/* sector sequence: empty,closed, write */
	zassert_equal(fixture->fs.ate_wra >> ADDR_SECT_SHIFT, 2, "unexpected write sector");
	check_content(max_id, &fixture->fs);

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	zassert_equal(fixture->fs.ate_wra >> ADDR_SECT_SHIFT, 2, "unexpected write sector");
	check_content(max_id, &fixture->fs);
}

/**
 * @brief Test case when storage becomes full, so only deletion is possible.
 */
ZTEST_F(bm_zms, test_zms_full_sector)
{
	int err;
	ssize_t len;
	uint32_t filling_id = 0;
	uint32_t data_read;

	fixture->config.sector_count = 3;

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	while (!nvm_is_full) {
		len = bm_zms_write(&fixture->fs, filling_id, &filling_id, sizeof(filling_id));
		wait_for_write();
		if (len == -ENOSPC) {
			break;
		}
		zassert_true(len == sizeof(filling_id), "bm_zms_write failed");
		filling_id++;
	}
	return;

	/* check whether can delete whatever from full storage */
	err = bm_zms_delete(&fixture->fs, 1);
	wait_for_write();
	zassert_true(err == 0, "bm_zms_delete call failure");

	/* the last sector is full now, test re-initialization */
	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	len = bm_zms_write(&fixture->fs, filling_id, &filling_id, sizeof(filling_id));
	wait_for_write();
	zassert_true(len == sizeof(filling_id), "bm_zms_write failed");

	/* sanitycheck on ZMS content */
	for (int i = 0; i <= filling_id; i++) {
		len = bm_zms_read(&fixture->fs, i, &data_read, sizeof(data_read));
		if (i == 1) {
			zassert_true(len == -ENOENT,
						 "bm_zms_read shouldn't found the entry");
		} else {
			zassert_true(len == sizeof(data_read), "bm_zms_read failed");
			zassert_equal(data_read, i, "read unexpected data");
		}
	}
}

ZTEST_F(bm_zms, test_delete)
{
	int err;
	ssize_t len;
	uint32_t filling_id;
	uint32_t data_read;
	uint32_t ate_wra;
	uint32_t data_wra;

	fixture->config.sector_count = 3;

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	for (filling_id = 0; filling_id < 10; filling_id++) {
		len = bm_zms_write(&fixture->fs, filling_id, &filling_id, sizeof(filling_id));
		wait_for_write();

		zassert_true(len == sizeof(filling_id), "bm_zms_write failed");

		if (filling_id != 0) {
			continue;
		}

		/* delete the first entry while it is the most recent one */
		err = bm_zms_delete(&fixture->fs, filling_id);
		wait_for_write();
		zassert_true(err == 0, "bm_zms_delete call failure");

		len = bm_zms_read(&fixture->fs, filling_id, &data_read, sizeof(data_read));
		zassert_true(len == -ENOENT, "bm_zms_read shouldn't found the entry");
	}

	/* delete existing entry */
	err = bm_zms_delete(&fixture->fs, 1);
	wait_for_write();
	zassert_true(err == 0, "bm_zms_delete call failure");

	len = bm_zms_read(&fixture->fs, 1, &data_read, sizeof(data_read));
	zassert_true(len == -ENOENT, "bm_zms_read shouldn't found the entry");

	ate_wra = fixture->fs.ate_wra;
	data_wra = fixture->fs.data_wra;
}

#if defined(CONFIG_BM_ZMS_LOOKUP_CACHE)
static size_t num_matching_cache_entries(uint64_t addr, bool compare_sector_only,
					 struct bm_zms_fs *fs)
{
	size_t num = 0;
	uint64_t mask = compare_sector_only ? ADDR_SECT_MASK : UINT64_MAX;

	for (int i = 0; i < CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE; i++) {
		if ((fs->lookup_cache[i] & mask) == addr) {
			num++;
		}
	}

	return num;
}

static size_t num_occupied_cache_entries(struct bm_zms_fs *fs)
{
	return CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE -
	       num_matching_cache_entries(ZMS_LOOKUP_CACHE_NO_ADDR, false, fs);
}
#endif

/*
 * Test that ZMS lookup cache is properly rebuilt on bm_zms_mount(), or initialized
 * to ZMS_LOOKUP_CACHE_NO_ADDR if the store is empty.
 */
ZTEST_F(bm_zms, test_zms_cache_init)
{
#if defined(CONFIG_BM_ZMS_LOOKUP_CACHE)
	int err;
	size_t num;
	uint64_t ate_addr;
	uint8_t data = 0;

	/* Test cache initialization when the store is empty */

	fixture->config.sector_count = 3;
	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	num = num_occupied_cache_entries(&fixture->fs);
	zassert_equal(num, 0, "uninitialized cache");

	/* Test cache update after bm_zms_write() */

	ate_addr = fixture->fs.ate_wra;
	err = bm_zms_write(&fixture->fs, 1, &data, sizeof(data));
	zassert_equal(err, sizeof(data), "bm_zms_write call failure");
	wait_for_write();

	num = num_occupied_cache_entries(&fixture->fs);
	zassert_equal(num, 1, "cache not updated after write");

	num = num_matching_cache_entries(ate_addr, false, &fixture->fs);
	zassert_equal(num, 1, "invalid cache entry after write");

	/* Test cache initialization when the store is non-empty */

	memset(fixture->fs.lookup_cache, 0xAA, sizeof(fixture->fs.lookup_cache));
	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	num = num_occupied_cache_entries(&fixture->fs);
	zassert_equal(num, 1, "uninitialized cache after restart");

	num = num_matching_cache_entries(ate_addr, false, &fixture->fs);
	zassert_equal(num, 1, "invalid cache entry after restart");
#endif
}

/*
 * Test that even after writing more ZMS IDs than the number of ZMS lookup cache
 * entries they all can be read correctly.
 */
ZTEST_F(bm_zms, test_zms_cache_collission)
{
#if defined(CONFIG_BM_ZMS_LOOKUP_CACHE)
	int err;
	uint16_t data;

	fixture->config.sector_count = 4;
	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	for (int id = 0; id < CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE + 1; id++) {
		data = id;
		err = bm_zms_write(&fixture->fs, id, &data, sizeof(data));
		zassert_equal(err, sizeof(data), "bm_zms_write call failure");
		wait_for_write();
	}

	for (int id = 0; id < CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE + 1; id++) {
		err = bm_zms_read(&fixture->fs, id, &data, sizeof(data));
		zassert_equal(err, sizeof(data), "bm_zms_read call failure");
		zassert_equal(data, id, "incorrect data read");
	}
#endif
}

/*
 * Test that ZMS lookup cache does not contain any address from gc-ed sector
 */
ZTEST_F(bm_zms, test_zms_cache_gc)
{
#if defined(CONFIG_BM_ZMS_LOOKUP_CACHE)
	int err;
	size_t num;
	uint16_t data = 0;

	fixture->config.sector_count = 3;
	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	/* Fill the first sector with writes of ID 1 */

	while (fixture->fs.data_wra + sizeof(data) + sizeof(struct zms_ate) <=
	       fixture->fs.ate_wra) {
		++data;
		err = bm_zms_write(&fixture->fs, 1, &data, sizeof(data));
		zassert_equal(err, sizeof(data), "bm_zms_write call failure");
		wait_for_write();
	}

	/* Verify that cache contains a single entry for sector 0 */

	num = num_matching_cache_entries(0ULL << ADDR_SECT_SHIFT, true, &fixture->fs);
	zassert_equal(num, 1, "invalid cache content after filling sector 0");

	/* Fill the second sector with writes of ID 2 */

	while ((fixture->fs.ate_wra >> ADDR_SECT_SHIFT) != 2) {
		++data;
		err = bm_zms_write(&fixture->fs, 2, &data, sizeof(data));
		zassert_equal(err, sizeof(data), "bm_zms_write call failure");
		wait_for_write();
	}

	/*
	 * At this point sector 0 should have been gc-ed. Verify that action is
	 * reflected by the cache content.
	 */

	num = num_matching_cache_entries(0ULL << ADDR_SECT_SHIFT, true, &fixture->fs);
	zassert_equal(num, 0, "not invalidated cache entries aftetr gc");

	num = num_matching_cache_entries(2ULL << ADDR_SECT_SHIFT, true, &fixture->fs);
	zassert_equal(num, 2, "invalid cache content after gc");
#endif
}

/*
 * Test ZMS lookup cache hash quality.
 */
ZTEST_F(bm_zms, test_zms_cache_hash_quality)
{
#if defined(CONFIG_BM_ZMS_LOOKUP_CACHE)
	const size_t MIN_CACHE_OCCUPANCY = CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE * 6 / 10;
	int err;
	size_t num;
	uint32_t id;
	uint16_t data;

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	/* Write ZMS IDs from 0 to CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE - 1 */

	for (int i = 0; i < CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE; i++) {
		id = i;
		data = 0;

		err = bm_zms_write(&fixture->fs, id, &data, sizeof(data));
		zassert_equal(err, sizeof(data), "bm_zms_write call failure");
		wait_for_write();
	}

	/* Verify that at least 60% cache entries are occupied */

	num = num_occupied_cache_entries(&fixture->fs);
	TC_PRINT("Cache occupancy: %u\n", (unsigned int)num);
	zassert_between_inclusive(num, MIN_CACHE_OCCUPANCY, CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE,
				  "too low cache occupancy - poor hash quality");

	err = bm_zms_clear(&fixture->fs);
	zassert_true(err == 0, "bm_zms_clear call failure");

	err = bm_zms_mount(&fixture->fs, &fixture->config);
	wait_for_mount();
	zassert_true(err == 0, "bm_zms_mount call failure");

	/* Write CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE ZMS IDs that form the following
	 * series: 0, 4, 8...
	 */

	for (int i = 0; i < CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE; i++) {
		id = i * 4;
		data = 0;

		err = bm_zms_write(&fixture->fs, id, &data, sizeof(data));
		zassert_equal(err, sizeof(data), "bm_zms_write call failure");
		wait_for_write();
	}

	/* Verify that at least 60% cache entries are occupied */

	num = num_occupied_cache_entries(&fixture->fs);
	TC_PRINT("Cache occupancy: %u\n", (unsigned int)num);
	zassert_between_inclusive(num, MIN_CACHE_OCCUPANCY, CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE,
				  "too low cache occupancy - poor hash quality");

#endif
}
