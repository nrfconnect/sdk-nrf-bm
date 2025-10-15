/* Copyright (c) 2018 Laczen
 * Copyright (c) 2024 BayLibre SAS
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ZMS: Zephyr Memory Storage
 */

#ifndef __BM_ZMS_PRIV_H_
#define __BM_ZMS_PRIV_H_

/*
 * MASKS AND SHIFT FOR ADDRESSES.
 * An address in bm_zms is an uint64_t where:
 * - high 4 bytes represent the sector number
 * - low 4 bytes represent the offset in a sector
 */
#define ADDR_SECT_MASK	 GENMASK64(63, 32)
#define ADDR_SECT_SHIFT	 32
#define ADDR_OFFS_MASK	 GENMASK64(31, 0)
#define SECTOR_NUM(x)	 FIELD_GET(ADDR_SECT_MASK, x)
#define SECTOR_OFFSET(x) FIELD_GET(ADDR_OFFS_MASK, x)

#if defined(CONFIG_BM_ZMS_CUSTOMIZE_BLOCK_SIZE)
#define ZMS_BLOCK_SIZE CONFIG_BM_ZMS_CUSTOM_BLOCK_SIZE
#else
#define ZMS_BLOCK_SIZE 32
#endif

#define ZMS_LOOKUP_CACHE_NO_ADDR GENMASK64(63, 0)
#define ZMS_HEAD_ID		 GENMASK(31, 0)

#define ZMS_VERSION_MASK	 GENMASK(7, 0)
#define ZMS_GET_VERSION(x)	 FIELD_GET(ZMS_VERSION_MASK, x)
#define ZMS_DEFAULT_VERSION	 1
#define ZMS_MAGIC_NUMBER	 0x42 /* murmur3a hash of "ZMS" (MSB) */
#define ZMS_MAGIC_NUMBER_MASK	 GENMASK(15, 8)
#define ZMS_GET_MAGIC_NUMBER(x)  FIELD_GET(ZMS_MAGIC_NUMBER_MASK, x)
#define ZMS_MIN_ATE_NUM		 5

#define ZMS_INVALID_SECTOR_NUM	 -1
#define ZMS_DATA_IN_ATE_SIZE	 8

/**
 * @ingroup zms_data_structures
 * BM_ZMS Allocation Table Entry (ATE) structure
 */
struct zms_ate {
	/** crc8 check of the entry */
	uint8_t crc8;
	/** cycle counter for non erasable devices */
	uint8_t cycle_cnt;
	/** data len within sector */
	uint16_t len;
	/** data id */
	uint32_t id;
	union {
		/** data field used to store small sized data */
		uint8_t data[8];
		struct {
			/** data offset within sector */
			uint32_t offset;
			union {
				/**
				 * crc for data: The data CRC is checked only when the whole data
				 * of the element is read.
				 * The data CRC is not checked for a partial read, as it is computed
				 * for the complete set of data.
				 */
				uint32_t data_crc;
				/**
				 * Used to store metadata information such as storage version.
				 */
				uint32_t metadata;
			};
		};
	};
} __packed;

/* BM_ZMS op-codes. */
typedef enum {
	ZMS_OP_NONE,
	ZMS_OP_INIT,  /* Initialize the module. */
	ZMS_OP_WRITE, /* Write a record to flash. */
	ZMS_OP_CLEAR, /* Clear all sectors. */
} zms_op_code_t;

typedef enum {
	ZMS_OP_INIT_START,
	ZMS_OP_INIT_ALL_OPEN_ADD_EMPTY_ATE,
	ZMS_OP_INIT_RECOVER_LAST_ATE,
	ZMS_OP_INIT_ADD_EMPTY_ATE_GC_DONE,
	ZMS_OP_INIT_ADD_EMPTY_ATE_GC_TODO,
	ZMS_OP_INIT_ADD_GC_DONE,
	ZMS_OP_INIT_GC_START,
	ZMS_OP_INIT_GC,
	ZMS_OP_INIT_DONE,
	ZMS_OP_WRITE_STARTUP,
	ZMS_OP_WRITE_EXECUTE,
	ZMS_OP_WRITE_CLOSE_SECTOR_GARBAGE,
	ZMS_OP_WRITE_CLOSE_SECTOR_ATE,
	ZMS_OP_WRITE_CLOSE_SECTOR_DONE,
	ZMS_OP_WRITE_ERASE_SECTOR,
	ZMS_OP_WRITE_GC,
	ZMS_OP_WRITE_DONE,
	ZMS_OP_CLEAR_START,
	ZMS_OP_CLEAR_EXECUTE,
	ZMS_OP_CLEAR_DONE,
} zms_write_step_t;

typedef enum {
	ZMS_OP_WRITE_SUB_STEP_NONE,
	ZMS_OP_WRITE_SUB_STEP_DATA1,
	ZMS_OP_WRITE_SUB_STEP_DATA2,
	ZMS_OP_WRITE_SUB_STEP_ATE1,
	ZMS_OP_WRITE_SUB_STEP_ATE2,
} zms_write_sub_step_t;

typedef enum {
	ZMS_OP_WRITE_GC_NONE,
	ZMS_OP_WRITE_GC_INIT,
	ZMS_OP_WRITE_GC_INIT_EMPTY_SECTOR,
	ZMS_OP_WRITE_GC_EXECUTE,
	ZMS_OP_WRITE_GC_DONE,
	ZMS_OP_WRITE_GC_BLK_MOVE,
	ZMS_OP_WRITE_GC_ATE_COPY,
	ZMS_OP_WRITE_GC_ATE_COPY_DONE,
	ZMS_OP_WRITE_GC_DONE_EMPTY_SECTOR,
} zms_gc_step_t;

typedef struct {
	zms_gc_step_t step;	 /* Current GC Step. */
	uint64_t gc_addr;	 /* Current GC address. */
	uint64_t gc_prev_addr;	 /* Previous GC address. */
	uint64_t sec_addr;	 /* Used to store next sector address. */
	uint64_t stop_addr;	 /* Address where GC ends. */
	uint64_t blk_mv_addr;	 /* Block address to be moved. */
	size_t blk_mv_len;	 /* Block length to be moved. */
	uint32_t previous_cycle; /* Cycle counter of the open sector. */
	uint32_t gc_count;	 /* Number of sectors being garbage collected. */
} gc_context_t;

typedef struct {
	uint64_t addr;	       /* Allocation Table Entry (ATE) write address. */
	uint64_t data_wra;     /* Data write address. */
	uint32_t sector_cycle; /* Sector cycle count. */
} init_context_t;

typedef struct __aligned(4) {
	__aligned(4) struct zms_ate ate_entry; /* ATE entry to write */
	__aligned(4) const void *data;	       /* Pointer to the data to write. */
	__aligned(4) const void *app_data;     /* Pointer to the application data. */
	zms_op_code_t op_code;		       /* The opcode for the operation. */
	uint32_t required_space;	       /* Required space for the operation. */
	zms_write_step_t step;		       /* The current step the operation is at. */
	zms_write_sub_step_t sub_step;	       /* The current sub-step the operation is at. */
	size_t len;			       /* Length of the current write. */
	size_t data_len;		       /* Length of the data to write. */
	size_t blen;			       /* Block size for the current write. */
	uint32_t id;			       /* BM_ZMS ATE ID */
	uint64_t addr;
	struct bm_zms_fs *fs;		       /* Pointer to the file system. */
	gc_context_t gc;		       /* Garbage collection context */
	init_context_t init;		       /* Initialization context */
	uint32_t clear_sector;		       /* Sector to clear */
	bool op_completed;	       /* The current operation completed. */
} zms_op_t;

#endif /* __BM_ZMS_PRIV_H_ */
