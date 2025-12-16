/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_sdk_srv_cgms_sst Session Start Time
 * @{
 * @ingroup ble_cgms
 *
 * @brief Continuous Glucose Monitoring Service SST module.
 *
 * @details This module implements parts of the Continuous Glucose Monitoring that relate to the
 *          Session Start Time characteristic. Events are propagated to this module from
 *          @ref ble_cgms using @ref cgms_sst_on_rw_auth_req.
 *
 */

#ifndef BLE_CGMS_SST_H__
#define BLE_CGMS_SST_H__

#include <ble.h>
#include <bm/bluetooth/services/ble_date_time.h>
#include <bm/bluetooth/services/ble_cgms.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Required data for setting the SST characteristic value. */
struct ble_cgms_sst {
	/** Date and time. */
	struct ble_date_time date_time;
	/** Time zone. */
	uint8_t time_zone;
	/** Daylight saving time. */
	uint8_t dst;
};

/**
 * @brief Add CGM Session Start Time characteristic.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] cgms_cfg Configuration structure.
 *
 * @retval NRF_SUCCESS If the characteristic was successfully added.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error is propagated.
 */
uint32_t cgms_sst_char_add(struct ble_cgms *cgms, const struct ble_cgms_config *cgms_cfg);

/**
 * @brief Set the Session Start Time characteristic value.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] sst Time and date that will be displayed in the session start time characteristic.
 *
 * @retval NRF_SUCCESS If the Session Start Time characteristic was successfully set.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
uint32_t cgms_sst_set(struct ble_cgms *cgms, struct ble_cgms_sst *sst);

/**
 * @brief Function for handling @ref BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST events.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] auth_req Authorize request event to be handled.
 */
void cgms_sst_on_rw_auth_req(struct ble_cgms *cgms,
			     const ble_gatts_evt_rw_authorize_request_t *auth_req);

#ifdef __cplusplus
}
#endif

#endif /* BLE_CGMS_SST_H__ */

/** @} */
