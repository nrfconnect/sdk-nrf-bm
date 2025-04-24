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

#ifndef NRF_BLE_CGMS_SST_H__
#define NRF_BLE_CGMS_SST_H__

#include <ble.h>
#include <bluetooth/services/ble_date_time.h>
#include <bluetooth/services/ble_cgms.h>

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
 * @brief Function for adding a characteristic for the Session Start Time.
 *
 * @param[in] cgms Instance of the CGM Service.
 *
 * @retval NRF_SUCCESS If the characteristic was successfully added.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
int cgms_sst_char_add(struct nrf_ble_cgms *cgms);

/**
 * @brief Function for setting the Session Run Time attribute.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] sst Time and date that will be displayed in the session start time attribute.
 *
 * @retval NRF_SUCCESS If the Session Run Time Attribute was successfully set.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
int cgms_sst_set(struct nrf_ble_cgms *cgms, struct ble_cgms_sst *sst);

/**
 * @brief Function for handling @ref BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST events.
 *
 * @param[in] p_cgms Instance of the CGM Service.
 * @param[in] p_auth_req Authorize request event to be handled.
 */
void cgms_sst_on_rw_auth_req(struct nrf_ble_cgms *cgms,
			     ble_gatts_evt_rw_authorize_request_t const *auth_req);

#ifdef __cplusplus
}
#endif

#endif /* NRF_BLE_CGMS_SST_H__ */

/** @} */
