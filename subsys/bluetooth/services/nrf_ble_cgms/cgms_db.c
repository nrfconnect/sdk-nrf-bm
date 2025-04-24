/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdbool.h>
#include <stdint.h>
#include "cgms_db.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_cgms, CONFIG_BLE_CGMS_LOG_LEVEL);

struct database_entry {
	bool in_use_flag;
	struct ble_cgms_rec record;
};

static struct database_entry db_entry[CGMS_DB_MAX_RECORDS];
static uint8_t db_crossref[CGMS_DB_MAX_RECORDS];
static uint16_t num_records;

int cgms_db_init(void)
{
	int i;

	for (i = 0; i < CGMS_DB_MAX_RECORDS; i++) {
		db_entry[i].in_use_flag = false;
		db_crossref[i]    = 0xFF;
	}

	num_records = 0;

	return NRF_SUCCESS;
}

uint16_t cgms_db_num_records_get(void)
{
	return num_records;
}

int cgms_db_record_get(struct ble_cgms_rec *rec, uint8_t record_num)
{
	if ((record_num >= num_records) || (num_records == 0)) {
		return NRF_ERROR_NOT_FOUND;
	}
	/* Copy record to the specified memory. */
	*rec = db_entry[db_crossref[record_num]].record;

	return NRF_SUCCESS;
}

int cgms_db_record_add(struct ble_cgms_rec *rec)
{
	int i;

	if (num_records == CGMS_DB_MAX_RECORDS) {
		return NRF_ERROR_NO_MEM;
	}

	/* Find next available database entry. */
	for (i = 0; i < CGMS_DB_MAX_RECORDS; i++) {
		if (!db_entry[i].in_use_flag) {
			db_entry[i].in_use_flag = true;
			db_entry[i].record = *rec;

			db_crossref[num_records] = i;
			num_records++;

			return NRF_SUCCESS;
		}
	}

	return NRF_ERROR_NO_MEM;
}


int cgms_db_record_delete(uint8_t record_num)
{
	int i;

	if (record_num >= num_records) {
		/* Deleting a non-existent record is not an error. */
		return NRF_SUCCESS;
	}

	/* Free entry. */
	db_entry[db_crossref[record_num]].in_use_flag = false;

	/* Decrease number of records. */
	num_records--;

	/* Remove cross reference index. */
	for (i = record_num; i < num_records; i++) {
		db_crossref[i] = db_crossref[i + 1];
	}

	return NRF_SUCCESS;
}
