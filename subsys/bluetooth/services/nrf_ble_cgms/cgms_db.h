/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_sdk_srv_cgms_db Continuous Glucose Monitoring Service database
 * @{
 * @ingroup ble_cgms
 *
 * @brief Continuous Glucose Monitoring Service database module.
 *
 * @details This module implements a database of stored glucose measurement values.
 *          This database is meant as an example of a database that the @ref ble_cgms can use.
 *          Replace this module if this implementation does not suit
 *          your application.
 */

#ifndef BLE_CGMS_DB_H__
#define BLE_CGMS_DB_H__

#include <bluetooth/services/ble_cgms.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Number of records that can be stored in the database. */
#define CGMS_DB_MAX_RECORDS 100

/**
 * @brief Function for initializing the glucose record database.
 *
 * @retval NRF_SUCCESS If the database was successfully initialized.
 */
int cgms_db_init(void);

/**
 * @brief Function for getting the number of records in the database.
 *
 * @return The number of records in the database.
 */
uint16_t cgms_db_num_records_get(void);

/**
 * @brief Function for getting a specific record from the database.
 *
 * @param[out] rec Pointer to the record structure to which the retrieved record is copied.
 * @param[in] record_num Index of the record to retrieve.
 *
 * @retval NRF_SUCCESS If the record was successfully retrieved.
 */
int cgms_db_record_get(struct ble_cgms_rec *rec, uint8_t record_num);

/**
 * @brief Function for adding a record at the end of the database.
 *
 * @param[in] rec Pointer to the record to add to the database.
 *
 * @retval NRF_SUCCESS If the record was successfully added to the database.
 */
int cgms_db_record_add(struct ble_cgms_rec *rec);

/**
 * @brief Function for deleting a database entry.
 *
 * @details This call deletes an record from the database.
 *
 * @param[in] record_num Index of the record to delete.
 *
 * @retval NRF_SUCCESS If the record was successfully deleted from the database.
 */
int cgms_db_record_delete(uint8_t record_num);

#ifdef __cplusplus
}
#endif

#endif /* BLE_CGMS_DB_H__ */

/** @} */
