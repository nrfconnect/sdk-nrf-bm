/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#define STORAGE_NODE DT_NODELABEL(storage0_partition)
#define BM_ZMS_PARTITION_OFFSET DT_REG_ADDR(STORAGE_NODE)
#define BM_ZMS_PARTITION_SIZE DT_REG_SIZE(STORAGE_NODE)

#if defined(CONFIG_SOFTDEVICE)
#include <nrf_sdh.h>
#include <nrf_soc.h>
#endif

#include <bm_zms.h>

static struct bm_zms_fs fs;
static bool nvm_is_full;

LOG_MODULE_REGISTER(app, CONFIG_BM_ZMS_SAMPLE_LOG_LEVEL);

static void wait_for_ongoing_writes(void)
{
	while (fs.ongoing_writes) {
#if defined(CONFIG_SOFTDEVICE)
		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
#endif
	}
}

static void wait_for_init(void)
{
	while (!fs.init_flags.initialized) {
#if defined(CONFIG_SOFTDEVICE)
		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
#endif
	}
}

#define IP_ADDRESS_ID 1
#define KEY_VALUE_ID  0xbeefdead
#define CNT_ID        2
#define LONG_DATA_ID  3

static int delete_and_verify_items(struct bm_zms_fs *fs, uint32_t id)
{
	int rc = 0;

	rc = bm_zms_delete(fs, id);
	if (rc) {
		goto error1;
	}
	wait_for_ongoing_writes();
	rc = bm_zms_get_data_length(fs, id);
	if (rc > 0) {
		goto error2;
	}

	return 0;
error1:
	LOG_ERR("Error while deleting item rc=%d", rc);
	return rc;
error2:
	LOG_ERR("Error, Delete failed item should not be present");
	return -1;
}

static int delete_basic_items(struct bm_zms_fs *fs)
{
	int rc = 0;

	rc = delete_and_verify_items(fs, IP_ADDRESS_ID);
	if (rc) {
		LOG_ERR("Error while deleting item %x rc=%d", IP_ADDRESS_ID, rc);
		return rc;
	}
	wait_for_ongoing_writes();
	rc = delete_and_verify_items(fs, KEY_VALUE_ID);
	if (rc) {
		LOG_ERR("Error while deleting item %x rc=%d", KEY_VALUE_ID, rc);
		return rc;
	}
	wait_for_ongoing_writes();
	rc = delete_and_verify_items(fs, CNT_ID);
	if (rc) {
		LOG_ERR("Error while deleting item %x rc=%d", CNT_ID, rc);
		return rc;
	}
	wait_for_ongoing_writes();
	rc = delete_and_verify_items(fs, LONG_DATA_ID);
	if (rc) {
		LOG_ERR("Error while deleting item %x rc=%d", LONG_DATA_ID, rc);
	}
	wait_for_ongoing_writes();

	return rc;
}

void bm_zms_sample_handler(bm_zms_evt_t const *p_evt)
{
	if (p_evt->evt_id == BM_ZMS_EVT_INIT) {
		if (p_evt->result) {
			LOG_ERR("BM_ZMS initialization failed with error %d", p_evt->result);
			return;
		}
	} else if (p_evt->evt_id == BM_ZMS_EVT_WRITE) {
		if (!p_evt->result) {
			return;
		}
		if (p_evt->result == -ENOSPC) {
			nvm_is_full = true;
			return;
		}
		LOG_ERR("BM_ZMS Error received %d", p_evt->result);
	} else {
		LOG_WRN("Unhandled BM_ZMS event ID %u", p_evt->evt_id);
	}
}

int main(void)
{
	int rc = 0;
	char buf[16];
	uint8_t key[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF}, longarray[128];
	uint32_t i_cnt = 0U;
	uint32_t i;
	uint32_t id = 0;
	ssize_t free_space = 0;

	LOG_INF("BM_ZMS sample started");

#if defined(CONFIG_SOFTDEVICE)
	rc = nrf_sdh_enable_request();
	if (rc) {
		LOG_ERR("Failed to enable SoftDevice, rc %u", rc);
		goto idle;
	}
#endif
	for (int n = 0; n < sizeof(longarray); n++) {
		longarray[n] = n;
	}

	rc = bm_zms_register(&fs, bm_zms_sample_handler);
	if (rc) {
		LOG_ERR("Something is wrong %u", rc);
		goto idle;
	}

	fs.offset = BM_ZMS_PARTITION_OFFSET;
	fs.sector_size = CONFIG_BM_ZMS_SECTOR_SIZE;
	fs.sector_count = (BM_ZMS_PARTITION_SIZE / CONFIG_BM_ZMS_SECTOR_SIZE);

	for (i = 0; i < CONFIG_BM_ZMS_ITERATIONS_MAX; i++) {
		rc = bm_zms_mount(&fs);
		if (rc) {
			LOG_ERR("Storage Init failed, rc=%d", rc);
			goto idle;
		}
		wait_for_init();

		LOG_INF("ITERATION: %u", i);
		/* IP_ADDRESS_ID is used to store an address, lets see if we can
		 * read it from non-volatile memory.
		 * Since we don't know the size, read the maximum possible
		 */
		rc = bm_zms_read(&fs, IP_ADDRESS_ID, &buf, sizeof(buf));
		if (rc > 0) {
			/* item was found, show it */
			buf[rc] = '\0';
			LOG_INF("ID: %u, IP Address: %s", IP_ADDRESS_ID, buf);
		}
		/* Rewriting ADDRESS IP even if we found it */
		strncpy(buf, "172.16.254.1", sizeof(buf) - 1);
		LOG_INF("Adding IP_ADDRESS %s at id %u", buf, IP_ADDRESS_ID);
		rc = bm_zms_write(&fs, IP_ADDRESS_ID, &buf, strlen(buf));
		if (rc < 0) {
			LOG_ERR("Error while writing Entry rc=%d", rc);
			goto idle;
		}
		wait_for_ongoing_writes();

		/* KEY_VALUE_ID is used to store a key/value pair , lets see if we can read
		 * it from storage.
		 */
		rc = bm_zms_read(&fs, KEY_VALUE_ID, &key, sizeof(key));
		if (rc > 0) { /* item was found, show it */
			LOG_INF("Id: %x", KEY_VALUE_ID);
			LOG_HEXDUMP_INF(key, sizeof(key), "Key:");
		}
		/* Rewriting KEY_VALUE even if we found it */
		LOG_INF("Adding key/value at id %x", KEY_VALUE_ID);
		rc = bm_zms_write(&fs, KEY_VALUE_ID, &key, sizeof(key));
		if (rc < 0) {
			LOG_ERR("Error while writing Entry rc=%d", rc);
			goto idle;
		}
		wait_for_ongoing_writes();

		/* CNT_ID is used to store the loop counter, lets see
		 * if we can read it from storage
		 */
		rc = bm_zms_read(&fs, CNT_ID, &i_cnt, sizeof(i_cnt));
		if (rc > 0) { /* item was found, show it */
			LOG_INF("Id: %d, loop_cnt: %u", CNT_ID, i_cnt);
			if ((i > 0) && (i_cnt != (i - 1))) {
				LOG_ERR("Error loop_cnt %u must be %d", i_cnt, i - 1);
				goto idle;
			}
		}
		LOG_INF("Adding counter at id %u", CNT_ID);
		rc = bm_zms_write(&fs, CNT_ID, &i, sizeof(i));
		if (rc < 0) {
			LOG_ERR("Error while writing Entry rc=%d", rc);
			goto idle;
		}
		wait_for_ongoing_writes();

		/* LONG_DATA_ID is used to store a larger dataset ,lets see if we can read
		 * it from non-volatile memory.
		 */
		rc = bm_zms_read(&fs, LONG_DATA_ID, &longarray, sizeof(longarray));
		if (rc > 0) {
			/* item was found, show it */
			LOG_INF("Id: %d", LONG_DATA_ID);
			LOG_HEXDUMP_INF(longarray, sizeof(longarray),
				      "Longarray:");
		}
		/* Rewrite the entry even if we found it */
		LOG_INF("Adding Longarray at id %d", LONG_DATA_ID);
		rc = bm_zms_write(&fs, LONG_DATA_ID, &longarray, sizeof(longarray));
		if (rc < 0) {
			LOG_ERR("Error while writing Entry rc=%d", rc);
			goto idle;
		}
		wait_for_ongoing_writes();

		/* Each DELETE_ITERATION delete all basic items */
		if (!(i % CONFIG_BM_ZMS_ITERATIONS_DELETE_INTERVAL) && (i)) {
			rc = delete_basic_items(&fs);
			if (rc) {
				goto idle;
			}
		}
	}

	if (i != CONFIG_BM_ZMS_ITERATIONS_MAX) {
		LOG_ERR("Error: Something went wrong at iteration %u rc=%d", i, rc);
		goto idle;
	}

	while (!nvm_is_full) {
		/* fill all storage */
		rc = bm_zms_write(&fs, id, &id, sizeof(uint32_t));
		if (rc < 0) {
			goto idle;
		}
		wait_for_ongoing_writes();
		id++;
	}

	/* Calculate free space and verify that it is 0 */
	free_space = bm_zms_calc_free_space(&fs);
	if (free_space < 0) {
		LOG_ERR("Error while computing free space, rc=%d", free_space);
		goto idle;
	}
	if (free_space > 0) {
		LOG_ERR("Error: free_space should be 0, computed %u", free_space);
		goto idle;
	}
	LOG_INF("Memory is full let's delete all items");
	/* Now delete all previously written items */
	for (uint32_t n = 0; n < id; n++) {
		rc = delete_and_verify_items(&fs, n);
		if (rc) {
			LOG_ERR("Error deleting at id %u", n);
			goto idle;
		}
	}
	rc = delete_basic_items(&fs);
	if (rc) {
		LOG_ERR("Error deleting basic items");
		goto idle;
	}

	/*
	 * Let's compute free space in storage.
	 */
	free_space = bm_zms_calc_free_space(&fs);
	if (free_space < 0) {
		LOG_ERR("Error while computing free space, rc=%d", free_space);
		goto idle;
	}
	LOG_INF("Free space in storage is %u bytes", free_space);

	/* Let's clean the storage now */
	rc = bm_zms_clear(&fs);
	if (rc < 0) {
		LOG_ERR("Error while cleaning the storage, rc=%d", rc);
		goto idle;
	}

	LOG_INF("BM_ZMS sample finished Successfully");

idle:
	/* Enter main loop. */
	while (true) {
		while (LOG_PROCESS()) {
			/* Empty. */
		}
#if defined(CONFIG_SOFTDEVICE)
		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
#endif
	}

	return 0;
}
