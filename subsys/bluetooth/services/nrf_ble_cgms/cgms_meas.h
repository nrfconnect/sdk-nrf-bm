/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_sdk_srv_cgms_meas Continuous Glucose Monitoring Service Measurement
 * @{
 * @ingroup ble_cgms
 * @brief Continuous Glucose Monitoring Service Measurement module.
 *
 * @details This module implements parts of the Continuous Glucose Monitoring that relate to the
 *          Measurement characteristic. Events are propagated to this module from @ref ble_cgms
 *          using @ref cgms_meas_on_write.
 *
 */

#ifndef NRF_BLE_CGMS_MEAS_H__
#define NRF_BLE_CGMS_MEAS_H__

#include <ble.h>
#include <bluetooth/services/ble_cgms.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function for adding a characteristic for the Continuous Glucose Monitoring Measurement.
 *
 * @param[in] cgms Instance of the CGM Service.
 *
 * @retval NRF_SUCCESS If the characteristic was successfully added.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
int cgms_meas_char_add(struct nrf_ble_cgms *cgms);

/**
 * @brief Function for sending a CGM Measurement.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] rec Measurement to be sent.
 * @param[in] count Number of measurements to encode.
 *
 * @retval NRF_SUCCESS If the measurement was successfully sent.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
int cgms_meas_send(struct nrf_ble_cgms *cgms, struct ble_cgms_rec *rec, uint8_t *count);

/**
 * @brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the BLE stack.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] evt_write Event received from the BLE stack.
 */
void cgms_meas_on_write(struct nrf_ble_cgms *cgms, const ble_gatts_evt_write_t *evt_write);

#ifdef __cplusplus
}
#endif

#endif /* NRF_BLE_CGMS_MEAS_H__ */

/** @} */
