/*
 * Copyright (c) 2013 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_lbs LED Button Service
 * @{
 * @brief LED Button Service.
 */
#ifndef BLE_LBS_H__
#define BLE_LBS_H__

#include <stdint.h>
#include <ble.h>
#include <bm/bluetooth/services/common.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_UUID_LBS_BASE	{ 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,                  \
				  0xDE, 0xEF, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00 }
#define BLE_UUID_LBS_SERVICE	 0x1523
#define BLE_UUID_LBS_BUTTON_CHAR 0x1524
#define BLE_UUID_LBS_LED_CHAR	 0x1525

/* Forward declaration */
struct ble_lbs;

/**
 * @brief Define a LED Button service instance.
 *
 * Define a LED Button service instance and register it as a Bluetooth event observer.
 */
#define BLE_LBS_DEF(_name)                                                                         \
	static struct ble_lbs _name;                                                               \
	extern void ble_lbs_on_ble_evt(const ble_evt_t *ble_evt, void *lbs_instance);              \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_lbs_on_ble_evt, &_name, HIGH)

/** @brief Default security configuration. */
#define BLE_LBS_CONFIG_SEC_MODE_DEFAULT                                                            \
	{                                                                                          \
		.lbs_button_char = {                                                               \
			.read = BLE_GAP_CONN_SEC_MODE_OPEN,                                        \
			.cccd_write = BLE_GAP_CONN_SEC_MODE_OPEN,                                  \
		},                                                                                 \
		.lbs_led_char = {                                                                  \
			.read = BLE_GAP_CONN_SEC_MODE_OPEN,                                        \
			.write = BLE_GAP_CONN_SEC_MODE_OPEN,                                       \
		},                                                                                 \
	}

enum ble_lbs_evt_type {
	/**
	 * @brief LED write event.
	 */
	BLE_LBS_EVT_LED_WRITE,
	/**
	 * @brief Error event.
	 */
	BLE_LBS_EVT_ERROR,
};

struct ble_lbs_evt {
	enum ble_lbs_evt_type evt_type;
	/**
	 * @brief Connection handle for which the event applies.
	 */
	uint16_t conn_handle;
	union {
		/** @ref BLE_LBS_EVT_LED_WRITE event data. */
		struct {
			/** Value to write */
			uint8_t value;
		} led_write;
		/** @ref BLE_LBS_EVT_ERROR event data. */
		struct {
			/** Error reason. */
			uint32_t reason;
		} error;
	};
};

/** @brief LED button Service event handler */
typedef void (*lbs_evt_handler_t)(struct ble_lbs *lbs, const struct ble_lbs_evt *lbs_evt);

/**
 * @brief LED Button Service init structure. This structure contains all options and data needed
 *        for initialization of the service.
 */
struct ble_lbs_config {
	/** @brief Event handler to be called when the LED Characteristic is written. */
	lbs_evt_handler_t evt_handler;
	/** Security configuration. */
	struct {
		/** LBS Button characteristic */
		struct {
			/** Security requirement for reading LBS button characteristic value. */
			ble_gap_conn_sec_mode_t read;
			/** Security requirement for writing LBS button characteristic CCCD. */
			ble_gap_conn_sec_mode_t cccd_write;
		} lbs_button_char;
		struct {
			/** Security requirement for reading LBS LED characteristic value. */
			ble_gap_conn_sec_mode_t read;
			/** Security requirement for wtiring LBS LED characteristic value. */
			ble_gap_conn_sec_mode_t write;
		} lbs_led_char;
	} sec_mode;
};

/**
 * @brief BLE Button Service structure.
 */
struct ble_lbs {
	/** @brief Handle of LED Button Service (as provided by the BLE stack). */
	uint16_t service_handle;
	/** @brief Handles related to the LED Characteristic. */
	ble_gatts_char_handles_t led_char_handles;
	/** @brief Handles related to the Button Characteristic. */
	ble_gatts_char_handles_t button_char_handles;
	/** @brief UUID type for the LED Button Service. */
	uint8_t uuid_type;
	/** @brief Event handler to be called when the LED Characteristic is written. */
	lbs_evt_handler_t evt_handler;
};

/**
 * @brief Function for initializing the LED Button Service.
 *
 * @param[out] lbs LED Button Service structure. This structure must be supplied by
 *                 the application. It is initialized by this function and will later
 *                 be used to identify this particular service instance.
 * @param[in] cfg  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was initialized successfully.
 * @retval NRF_ERROR_NULL If @p lbs or @p cfg is NULL.
 * @return In addition, this function may return any error
 *         returned by the following SoftDevice functions:
 *         - @ref sd_ble_gatts_service_add()
 *         - @ref sd_ble_gatts_characteristic_add()
 *         - @ref sd_ble_uuid_vs_add()
 */
uint32_t ble_lbs_init(struct ble_lbs *lbs, const struct ble_lbs_config *cfg);

/**
 * @brief Function for handling the application's BLE stack events.
 *
 * @details This function handles all events from the BLE stack that are of interest to the LED
 *          Button Service.
 *
 * @param[in] ble_evt      Event received from the BLE stack.
 * @param[in] lbs_instance LED Button Service structure.
 */
void ble_lbs_on_ble_evt(const ble_evt_t *ble_evt, void *lbs_instance);

/**
 * @brief Function for sending a button state notification.
 *
 * @param[in] lbs           LED Button Service structure.
 * @param[in] conn_handle   Handle of the peripheral connection to which the button state
 *                          notification will be sent.
 * @param[in] button_state  New button state.
 *
 * @retval NRF_SUCCESS If the notification was sent successfully.
 * @retval NRF_ERROR_NULL If @p lbs is NULL.
 * @return In addition, this function may return any error
 *         returned by the following SoftDevice functions:
 *         - @ref sd_ble_gatts_hvx()
 */
uint32_t ble_lbs_on_button_change(struct ble_lbs *lbs, uint16_t conn_handle, uint8_t button_state);

#ifdef __cplusplus
}
#endif

#endif /* BLE_LBS_H__ */

/** @} */
