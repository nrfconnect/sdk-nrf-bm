/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/sys/crc.h>
#include <bm/storage/bm_rmem.h>


#define DT_DRV_COMPAT zephyr_retained_ram

#define RETAINED_RAM_ADDRESS DT_REG_ADDR(DT_PARENT(DT_INST(0, DT_DRV_COMPAT)))
#define RETAINED_RAM_SIZE DT_REG_SIZE(DT_PARENT(DT_INST(0, DT_DRV_COMPAT)))

#define TLV_HEADER_SIZE 4 /* type + len */

/* Constants matching bm_rmem.c */
#define CRC_TLV_LEN_OFFSET (sizeof(uint32_t) + TLV_HEADER_SIZE)
#define CRC_TLV_SIZE (CRC_TLV_LEN_OFFSET + sizeof(uint16_t))

/* Expected RAM pattern created by bm_rmem API */
static const uint8_t pattern_crc_tl[] = {
	/* Offset 0-3: CRC TLV header (type=0x0002, len=0x000A */
	0x02, 0x00, 0x06, 0x00,
};

/**
 * @brief Test suite for bm_rmem module
 *
 * This test suite tests the bm_rmem clipboard functionality
 * enabled via CONFIG_NCS_BM_RMEM_CLIPBOARD=y
 */
ZTEST_SUITE(bm_rmem_tests, NULL, NULL, NULL, NULL, NULL);


static void blure_retention_area(void)
{
	uint8_t *ptr = (uint8_t *)RETAINED_RAM_ADDRESS;
	size_t i;
	uint8_t pattern = 0;

	for (i = 0; i < RETAINED_RAM_SIZE; i++) {
		ptr[i] = pattern;
		pattern++;
	}
}

/**
 * @brief Test: Verify bm_rmem content can be summarized by writing crc32_ieee
 *
 * This test verifies that CRC32 IEEE checksum can be calculated
 * and written to summarize the bm_rmem content.
 */
ZTEST(bm_rmem_tests, test_bm_rmem_write_and_commit)
{
	int ret;
	struct bm_retained_clipboard_ctx ctx;
	static const uint8_t pattern_data_1[] = {
		/* Offset 8-9: Data length (15 bytes = 0x000F) */
		0x0F, 0x00,
		/* Offset 10-13: Data TLV header (type=0x1789, len=0x0009) */
		0x89, 0x17, 0x09, 0x00,
		/* Offset 14-22: Test data */
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
	};
	const uint8_t *test_data = &pattern_data_1[6];
	const uint16_t tlv_type = 0x1789;

	/* Populate retention area with pattern (0-255 sequence) */
	blure_retention_area();

	/* Initialize bm_rmem for writing */
	ret = bm_rmem_writer_init(&ctx);
	zassert_equal(ret, 0, "bm_rmem initialization failed, ret=%d", ret);

	/* Write test data first */
	ret = bm_rmem_data_write(&ctx, tlv_type, test_data, 9);
	zassert_equal(ret, 0, "bm_rmem TLV write failed, ret=%d", ret);

	/* Write CRC32 IEEE checksum to summarize content */
	ret = bm_rmem_crc32_write(&ctx);
	zassert_equal(ret, 0, "bm_rmem CRC32 write failed, ret=%d", ret);

	/* Verify RAM memory content against expected data vector */
	uint8_t *ram_ptr = (uint8_t *)RETAINED_RAM_ADDRESS;

	/* Verify RAM content matches expected pattern (excluding CRC bytes) */
	/* Compare bytes 0-3 (CRC TLV header) */
	zassert_mem_equal(ram_ptr, pattern_crc_tl, sizeof(pattern_crc_tl),
			  "CRC TLV header mismatch");

	/* Skip CRC32 bytes (offset 4-7) - verify separately */
	/* Compare bytes 8-9 (data length) */
	zassert_mem_equal(ram_ptr + CRC_TLV_LEN_OFFSET,
			  pattern_data_1,
			  sizeof(pattern_data_1),
			  "clipboard content mismatch");

	/* Verify CRC32 checksum separately */
	uint32_t expected_crc = crc32_ieee(pattern_data_1, sizeof(pattern_data_1));
	uint32_t actual_crc = *(uint32_t *)(ram_ptr + sizeof(pattern_crc_tl));

	zassert_equal(actual_crc, expected_crc,
		      "CRC32 mismatch: expected 0x%08x, got 0x%08x",
		      expected_crc, actual_crc);
}

ZTEST(bm_rmem_tests, test_bm_rmem_read_pattern)
{
	int ret;
	struct bm_retained_clipboard_ctx ctx;
	static const uint8_t pattern_data_2[] = {
		/* Offset 0-3: CRC TLV header */
		0x02, 0x00, 0x06, 0x00,
		/* Offset 4-7: CRC32 checksum (empty/zero) */
		0x00, 0x00, 0x00, 0x00,
		/* Offset 8-9: Data length (15 bytes = 0x000F) */
		0x18, 0x00,
		/* Offset 10-13: Data TLV header (type=0x1789, len=0x0009) */
		0x89, 0x17, 0x09, 0x00,
		/* Offset 14-22: Test data */
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,

		0x32, 0x00, 0x01, 0x00,

		0x5a,

		0x33, 0x00, 0x00, 0x00,
	};

	struct bm_rmem_data_desc desc;

	uint32_t crc = crc32_ieee(pattern_data_2 + CRC_TLV_LEN_OFFSET,
				  sizeof(pattern_data_2) - CRC_TLV_LEN_OFFSET);

	memset((uint8_t *)RETAINED_RAM_ADDRESS, 0, RETAINED_RAM_SIZE);

	memcpy((uint8_t *)RETAINED_RAM_ADDRESS, pattern_data_2, sizeof(pattern_data_2));
	memcpy(((uint8_t *)RETAINED_RAM_ADDRESS + 4), &crc, sizeof(crc));

	ret = bm_rmem_reader_init(&ctx);
	zassert_equal(ret, 0, "bm_rmem initialization failed, ret=%d", ret);

	desc.type = 0x1789;

	ret = bm_rmem_data_get(&ctx, &desc);
	zassert_equal(ret, 0, "bm_rmem TLV read failed, ret=%d", ret);
	zassert_equal(desc.len, 9, "data length mismatch");
	zassert_mem_equal(desc.data, pattern_data_2 + 14, desc.len, "data mismatch");

	desc.type = 0x32;

	ret = bm_rmem_data_get(&ctx, &desc);
	zassert_equal(ret, 0, "bm_rmem TLV read failed, ret=%d", ret);
	zassert_equal(desc.len, 1, "data length mismatch");
	zassert_mem_equal(desc.data, pattern_data_2 + 27, desc.len, "data mismatch");

	desc.type = 0x33;
	ret = bm_rmem_data_get(&ctx, &desc);
	zassert_equal(ret, 0, "bm_rmem TLV read failed, ret=%d", ret);
	zassert_equal(desc.len, 0, "data length mismatch");

	desc.type = 0x172;
	ret = bm_rmem_data_get(&ctx, &desc);
	zassert_equal(ret, -ENOENT, "bm_rmem TLV read should fail, ret=%d", ret);
}

ZTEST(bm_rmem_tests, test_bm_rmem_write_and_read)
{
	int ret;
	struct bm_retained_clipboard_ctx ctx, ctx2;
	struct pattern_data_s {
		uint16_t type;
		uint16_t len;
		uint8_t *data;
	};

	static const struct pattern_data_s pattern_array[] = {
		{0x1789, 9, (uint8_t *)"123456789"},
		{0x32, 1, (uint8_t *)"a"},
		{0x33, 0, NULL},
		{0x172, 0, NULL},
	};

	/* Populate retention area with pattern (0-255 sequence) */
	blure_retention_area();

	/* Initialize bm_rmem for writing */
	ret = bm_rmem_writer_init(&ctx);
	zassert_equal(ret, 0, "bm_rmem initialization failed, ret=%d", ret);

	/* Write test data first */
	for (int i = 0; i < ARRAY_SIZE(pattern_array); i++) {
		ret = bm_rmem_data_write(&ctx, pattern_array[i].type, pattern_array[i].data,
					 pattern_array[i].len);
		zassert_equal(ret, 0, "bm_rmem TLV write failed, ret=%d", ret);
	}

	/* Write CRC32 IEEE checksum to summarize content */
	ret = bm_rmem_crc32_write(&ctx);
	zassert_equal(ret, 0, "bm_rmem CRC32 write failed, ret=%d", ret);


	ret = bm_rmem_reader_init(&ctx2);
	zassert_equal(ret, 0, "bm_rmem initialization failed, ret=%d", ret);

	/* Read test data */
	for (int i = 0; i < ARRAY_SIZE(pattern_array); i++) {
		struct bm_rmem_data_desc desc;

		desc.type = pattern_array[i].type;

		ret = bm_rmem_data_get(&ctx2, &desc);
		zassert_equal(ret, 0, "bm_rmem TLV 0x%02x read failed, ret=%d", desc.type, ret);
		zassert_equal(desc.len, pattern_array[i].len, "data length mismatch");
		zassert_mem_equal(desc.data, pattern_array[i].data, desc.len, "data mismatch");
	}
}

ZTEST(bm_rmem_tests, test_bm_rmem_write_overflow)
{
	int ret;
	struct bm_retained_clipboard_ctx ctx;
	const uint8_t pattern_data_3[] = "123456789";
	uint16_t type = 10;

	/* Populate retention area with pattern (0-255 sequence) */
	blure_retention_area();

	/* Initialize bm_rmem for writing */
	ret = bm_rmem_writer_init(&ctx);
	zassert_equal(ret, 0, "bm_rmem initialization failed, ret=%d", ret);

	uint32_t expected_size = CRC_TLV_SIZE + strlen(pattern_data_3) + TLV_HEADER_SIZE;

	for (;
	     expected_size <= RETAINED_RAM_SIZE;
	     expected_size += strlen(pattern_data_3) + TLV_HEADER_SIZE
	    ) {
		/* Write test data first */
		ret = bm_rmem_data_write(&ctx, type, pattern_data_3, strlen(pattern_data_3));
		zassert_equal(ret, 0, "bm_rmem TLV 0x%02x write failed, ret=%d", type, ret);
		type++;
	}

	ret = bm_rmem_data_write(&ctx, type, pattern_data_3, strlen(pattern_data_3));
	zassert_equal(ret, -ENOMEM, "bm_rmem data write no. %d shall fail as -ENOMEM, ret=%d",
		      type - 9, ret);
}
