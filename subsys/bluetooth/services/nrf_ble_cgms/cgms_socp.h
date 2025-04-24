/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_sdk_srv_cgms_socp Specific Operations Control Point
 * @{
 * @ingroup ble_cgms
 * @brief Continuous Glucose Monitoring Service SOCP module.
 *
 * @details This module implements parts of the Continuous Glucose Monitoring that relate to the
 *          Specific Operations Control Point. Events are propagated to this module from
 *          @ref ble_cgms using @ref cgms_socp_on_rw_auth_req.
 *
 */

#ifndef NRF_BLE_CGMS_SOCP_H__
#define NRF_BLE_CGMS_SOCP_H__

#include <ble.h>
#include <bluetooth/services/ble_cgms.h>

#ifdef __cplusplus
extern "C" {
#endif


/** @brief Specific Operation Control Point value. */
struct ble_cgms_socp_value {
	/** Opcode. */
	uint8_t opcode;
	/** Length of the operand. */
	uint8_t operand_len;
	/** Pointer to the operand. */
	uint8_t *operand;
};

/**
 * @brief Function for adding a characteristic for the Specific Operations Control Point.
 *
 * @param[in] cgms Instance of the CGM Service.
 *
 * @retval NRF_SUCCESS If the characteristic was successfully added.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
int cgms_socp_char_add(struct nrf_ble_cgms *cgms);

/**
 * @brief Function for handling @ref BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST events.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] auth_req Authorize request event to be handled.
 */
void cgms_socp_on_rw_auth_req(struct nrf_ble_cgms *cgms,
			      ble_gatts_evt_rw_authorize_request_t const *auth_req);

#ifdef __cplusplus
}
#endif

#endif /* NRF_BLE_CGMS_SOCP_H__ */

/** @} */
