/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_sdk_srv_cgms_socp Specific Ops Control Point
 * @{
 * @ingroup ble_cgms
 * @brief Continuous Glucose Monitoring Service SOCP module.
 *
 * @details This module implements parts of the Continuous Glucose Monitoring that relate to the
 *          Specific Ops Control Point. Events are propagated to this module from
 *          @ref ble_cgms using @ref cgms_socp_on_rw_auth_req.
 *
 */

#ifndef BLE_CGMS_SOCP_H__
#define BLE_CGMS_SOCP_H__

#include <ble.h>
#include <bm/bluetooth/services/ble_cgms.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Add CGM Specific Ops Control Point characteristic.
 *
 * @param[in] cgms Instance of the CGM Service.
 *
 * @retval NRF_SUCCESS If the characteristic was successfully added.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error is propagated.
 */
uint32_t cgms_socp_char_add(struct ble_cgms *cgms);

/**
 * @brief Function for handling @ref BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST events.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] auth_req Authorize request event to be handled.
 */
void cgms_socp_on_rw_auth_req(struct ble_cgms *cgms,
			      const ble_gatts_evt_rw_authorize_request_t *auth_req);

#ifdef __cplusplus
}
#endif

#endif /* BLE_CGMS_SOCP_H__ */

/** @} */
