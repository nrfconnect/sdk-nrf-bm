/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_hids Human Interface Device Service
 * @{
 * @brief Human Interface Device Service.
 */
#ifndef BLE_HIDS_H__
#define BLE_HIDS_H__

#include <stdint.h>
#include <stdbool.h>
#include <ble.h>
#include <ble_gap.h>
#include <ble_gatts.h>
#include <nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Define a HID service instance.
 *
 * Define a HID service instance and register it as a Bluetooth event observer.
 */
#define BLE_HIDS_DEF(_name)                                                                        \
	static struct ble_hids _name;                                                              \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_hids_on_ble_evt, &_name, 0)

/**
 * @brief HID boot keyboard input report maximum size, in bytes.
 */
#define BLE_HIDS_BOOT_KB_INPUT_REP_MAX_SIZE 8
/**
 * @brief HID boot keyboard output report maximum size, in bytes.
 */
#define BLE_HIDS_BOOT_KB_OUTPUT_REP_MAX_SIZE 1
/**
 * @brief HID boot mouse input report maximum size, in bytes.
 */
#define BLE_HIDS_BOOT_MOUSE_INPUT_REP_MAX_SIZE 8

/**
 * @brief HID report types as defined in the Report Reference Characteristic descriptor.
 */
enum ble_hids_report_type {
	/**
	 * @brief Reserved.
	 */
	BLE_HIDS_REPORT_TYPE_RESERVED = 0x00,
	/**
	 * @brief Input report.
	 */
	BLE_HIDS_REPORT_TYPE_INPUT = 0x01,
	/**
	 * @brief Output report.
	 */
	BLE_HIDS_REPORT_TYPE_OUTPUT = 0x02,
	/**
	 * @brief Feature report.
	 */
	BLE_HIDS_REPORT_TYPE_FEATURE = 0x03
};

/**
 * @brief BLE HID service boot keyboard input report.
 */
struct ble_hids_boot_keyboard_input_report {
	/**
	 * @brief Key modifier.
	 */
	enum __packed {
		KEY_LEFT_CTRL = 0x01,
		KEY_LEFT_SHIFT = 0x02,
		KEY_LEFT_ALT = 0x04,
		KEY_LEFT_GUI = 0x08,
		KEY_RIGHT_CTRL = 0x10,
		KEY_RIGHT_SHIFT = 0x20,
		KEY_RIGHT_ALT = 0x40,
		KEY_RIGHT_GUI = 0x80,
	} modifier;
	/**
	 * @brief Reserved, zero.
	 */
	uint8_t reserved;
	/**
	 * @brief Key codes.
	 */
	uint8_t keycode[6];
};
BUILD_ASSERT(sizeof(struct ble_hids_boot_keyboard_input_report) ==
	     BLE_HIDS_BOOT_KB_INPUT_REP_MAX_SIZE);

/**
 * @brief BLE HID service boot mouse input report.
 */
struct ble_hids_boot_mouse_input_report {
	/**
	 * @brief Buttons mask.
	 */
	uint8_t buttons;
	/**
	 * @brief Delta X.
	 */
	int8_t delta_x;
	/**
	 * @brief Delta Y.
	 */
	int8_t delta_y;
	/**
	 * @brief Optional data.
	 */
	uint8_t data[5];
};
BUILD_ASSERT(sizeof(struct ble_hids_boot_mouse_input_report) ==
	     BLE_HIDS_BOOT_MOUSE_INPUT_REP_MAX_SIZE);

/**
 * @brief HID service characteristic ID.
 */
struct ble_hids_char_id {
	/**
	 * @brief Characteristic UUID.
	 */
	uint16_t uuid;
	/**
	 * @brief Type of report.
	 *
	 * Only used when @ref uuid is BLE_UUID_REPORT_CHAR.
	 */
	enum ble_hids_report_type rep_type;
	/**
	 * @brief Index of the characteristic
	 *
	 * Only used when @ref uuid is BLE_UUID_REPORT_CHAR.
	 */
	uint8_t rep_index;
};

/**
 * @brief HID Service event type.
 */
enum ble_hids_evt_type {
	/**
	 * @brief Suspend command received.
	 */
	BLE_HIDS_EVT_HOST_SUSP,
	/**
	 * @brief Exit suspend command received.
	 */
	BLE_HIDS_EVT_HOST_EXIT_SUSP,
	/**
	 * @brief Notification enabled.
	 */
	BLE_HIDS_EVT_NOTIF_ENABLED,
	/**
	 * @brief Notification disabled.
	 */
	BLE_HIDS_EVT_NOTIF_DISABLED,
	/**
	 * @brief A new value has been written to an Report characteristic.
	 */
	BLE_HIDS_EVT_REP_CHAR_WRITE,
	/**
	 * @brief Boot mode entered.
	 */
	BLE_HIDS_EVT_BOOT_MODE_ENTERED,
	/**
	 * @brief Report mode entered.
	 */
	BLE_HIDS_EVT_REPORT_MODE_ENTERED,
	/**
	 * @brief Read with response.
	 */
	BLE_HIDS_EVT_REPORT_READ
};

/**
 * @brief HID service event.
 */
struct ble_hids_evt {
	/**
	 * @brief Event type.
	 */
	enum ble_hids_evt_type evt_type;
	/**
	 * @brief BLE event.
	 */
	const ble_evt_t *ble_evt;
	/**
	 * @brief Event parameters.
	 */
	union {
		/**
		 * @brief Parameters for
		 * - @ref BLE_HIDS_EVT_NOTIF_ENABLED
		 * - @ref BLE_HIDS_EVT_NOTIF_DISABLED
		 */
		struct {
			/**
			 * @brief Characteristic ID.
			 */
			struct ble_hids_char_id char_id;
		} notification;
		/**
		 * @brief Parameters for @ref BLE_HIDS_EVT_REP_CHAR_WRITE.
		 */
		struct {
			/**
			 * @brief Characteristic ID.
			 */
			struct ble_hids_char_id char_id;
			/**
			 * @brief Offset of the write operation.
			 */
			uint16_t offset;
			/**
			 * @brief Length of the write operation.
			 */
			uint16_t len;
			/**
			 * @brief Incoming data.
			 */
			const void *data;
		} char_write;
		/**
		 * @brief Parameters for @ref BLE_HIDS_EVT_REPORT_READ.
		 */
		struct {
			/**
			 * @brief Characteristic ID.
			 */
			struct ble_hids_char_id char_id;
		} char_auth_read;
	} params;
};

/**
 * @brief HID report characteristic configuration.
 */
struct ble_hids_report_config {
	/**
	 * @brief Report type.
	 */
	enum ble_hids_report_type report_type;
	/**
	 * @brief Report ID.
	 */
	uint8_t report_id;
	/**
	 * @brief Characteristic length.
	 */
	uint16_t len;
	/**
	 * @brief Security requirements for Report characteristic.
	 */
	struct {
		/**
		 * @brief Security requirements for Read operations.
		 */
		ble_gap_conn_sec_mode_t read;
		/**
		 * @brief Security requirements for Write operations.
		 */
		ble_gap_conn_sec_mode_t write;
		/**
		 * @brief Security requirements for CCCD Write operations.
		 */
		ble_gap_conn_sec_mode_t cccd_write;
	} sec;
};

/**
 * @brief HID Report characteristic.
 */
struct ble_hids_report {
	/**
	 * @brief Report characteristic handles.
	 */
	ble_gatts_char_handles_t char_handles;
	/**
	 * @brief Report reference descriptor handle.
	 */
	uint16_t ref_handle;
	/**
	 * @brief Maximum report length.
	 */
	uint16_t max_len;
};

/* Forward declaration */
struct ble_hids;

/**
 * @brief HID service event handler type.
 */
typedef void (*ble_hids_evt_handler_t)(struct ble_hids *hids, const struct ble_hids_evt *evt);

/**
 * @brief HID service configuration.
 */
struct ble_hids_config {
	/**
	 * @brief HID service event handler.
	 */
	ble_hids_evt_handler_t evt_handler;
	/**
	 * @brief Input report characteristic configuration.
	 */
	const struct ble_hids_report_config *input_report;
	/**
	 * @brief Output report characteristic configuration.
	 */
	const struct ble_hids_report_config *output_report;
	/**
	 * @brief Feature report characteristic configuration.
	 */
	const struct ble_hids_report_config *feature_report;
	/**
	 * @brief Array of services to include in HID service.
	 */
	uint16_t *included_services_array;
	/**
	 * @brief Number of Input Report characteristics in @ref input_report.
	 */
	uint8_t input_report_count;
	/**
	 * @brief Number of Output Report characteristics in @ref output_report.
	 */
	uint8_t output_report_count;
	/**
	 * @brief Number of Feature Report characteristics in @ref feature_report.
	 */
	uint8_t feature_report_count;
	/**
	 * @brief Number of services to include in HID service.
	 */
	uint8_t included_services_count;
	/**
	 * @brief HID Report Map characteristic configuration.
	 */
	struct {
		/**
		 * @brief Report map data.
		 */
		uint8_t *data;
		/**
		 * @brief Length of report map data.
		 */
		uint16_t len;
		/**
		 * @brief External Report Reference descriptors (optional).
		 */
		const ble_uuid_t *ext_rep_ref;
		/**
		 * @brief Number of External Report Reference descriptors in @ref ext_rep_ref.
		 */
		uint8_t ext_rep_ref_count;
		/**
		 * @brief Security requirements.
		 */
		struct {
			/**
			 * @brief Security requirements for the Read operation.
			 */
			ble_gap_conn_sec_mode_t read;
		} sec;
	} report_map;
	/**
	 * @brief HID information characteristic configuration.
	 */
	struct {
		/**
		 * @brief HID version specification.
		 *
		 * Binary coded decimal.
		 */
		uint16_t bcd_hid;
		/**
		 * @brief Country code, if device is localized.
		 */
		uint8_t b_country_code;
		/**
		 * @brief HID information flags.
		 */
		struct {
			/**
			 * @brief Device is normally connectable.
			 */
			uint8_t normally_connectable: 1;
			/**
			 * @brief Device can be waked remotely.
			 */
			uint8_t remote_wake: 1;
			/**
			 * @brief Reserved.
			 */
			uint8_t reserved: 6;
		} bcd_flags;
		/**
		 * @brief Security requirements for HID information characteristic.
		 */
		struct {
			/**
			 * @brief Security requirements for the Read operation.
			 */
			ble_gap_conn_sec_mode_t read;
		} sec;
	} hid_information;
};

/**
 * @brief HID service structure.
 */
struct ble_hids {
	/**
	 * @brief HID Service event handler.
	 */
	ble_hids_evt_handler_t evt_handler;
	/**
	 * @brief Handle of HID Service.
	 */
	uint16_t service_handle;
	/**
	 * @brief Control point characteristic handles.
	 */
	ble_gatts_char_handles_t control_point_handles;
	/**
	 * @brief Protocol Mode characteristic handles.
	 */
	ble_gatts_char_handles_t protocol_mode_handles;
	/**
	 * @brief Report Map characteristic handles.
	 */
	ble_gatts_char_handles_t rep_map_handles;
	/**
	 * @brief HID information characteristic handles.
	 */
	ble_gatts_char_handles_t hid_information_handles;
#if CONFIG_BLE_HIDS_BOOT_KEYBOARD
	/**
	 * @brief Boot Keyboard Input Report characteristic handles.
	 */
	ble_gatts_char_handles_t boot_kb_inp_rep_handles;
	/**
	 * @brief Boot Keyboard Output Report characteristic handles.
	 */
	ble_gatts_char_handles_t boot_kb_outp_rep_handles;
#endif
#if CONFIG_BLE_HIDS_BOOT_MOUSE
	/**
	 * @brief Boot Mouse Input Report characteristic handles.
	 */
	ble_gatts_char_handles_t boot_mouse_inp_rep_handles;
#endif
	/**
	 * @brief Number of input reports.
	 */
	uint8_t input_report_count;
	/**
	 * @brief Number of output reports.
	 */
	uint8_t output_report_count;
	/**
	 * @brief Number of feature reports.
	 */
	uint8_t feature_report_count;
	/**
	 * @brief Input reports.
	 */
	struct ble_hids_report input_report[CONFIG_BLE_HIDS_MAX_INPUT_REP];
	/**
	 * @brief Output reports.
	 */
	struct ble_hids_report output_report[CONFIG_BLE_HIDS_MAX_OUTPUT_REP];
	/**
	 * @brief Feature reports.
	 */
	struct ble_hids_report feature_report[CONFIG_BLE_HIDS_MAX_FEATURE_REP];
};

/**
 * @brief HID service event handler for SoftDevice BLE events.
 *
 * @param ble_evt SoftDevice BLE event.
 * @param ctx Context.
 */
void ble_hids_on_ble_evt(const ble_evt_t *ble_evt, void *ctx);

/**
 * @brief Set the event handler for HID service.
 *
 * @param hids HID service.
 * @param handler HID service event handler.
 *
 * @retval 0 On success.
 * @retval NRF_ERROR_NULL If @p hids or @p handler are @c NULL.
 */
uint32_t ble_hids_event_handler_set(struct ble_hids *hids, ble_hids_evt_handler_t handler);

/**
 * @brief Initialize HID service.
 *
 * @param hids HID service.
 * @param hids_config HID service configuration.
 *
 * @retval 0 On success.
 * @retval NRF_ERROR_NULL If @p hids or @p hids_config are @c NULL.
 * @retval NRF_ERROR_INVALID_PARAM Invalid parameters.
 */
uint32_t ble_hids_init(struct ble_hids *hids, const struct ble_hids_config *hids_config);

/**
 * @brief Send an input report.
 *
 * @param hids HID service.
 * @param conn_handle Connection handle.
 * @param rep_index Index of the input report characteristic.
 * @param data Input report data.
 * @param len Input report data length.
 *
 * @retval 0 On success.
 * @retval NRF_ERROR_NULL If @p hids or @p data are @c NULL.
 * @retval NRF_ERROR_NOT_FOUND Unknown connection handle.
 * @retval NRF_ERROR_INVALID_PARAM Report index @p rep_index is invalid.
 * @retval NRF_ERROR_DATA_SIZE Report data length @p len exceeds maximum characteristic length.
 */
uint32_t ble_hids_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle, uint8_t rep_index,
			       const void *data, uint16_t len);

/**
 * @brief Send a boot keyboard input report.
 *
 * @param hids HID service.
 * @param conn_handle Connection handle.
 * @param report Boot keyboard input report.
 *
 * @retval 0 On success.
 * @retval NRF_ERROR_NULL If @p hids or @p report are @c NULL.
 * @retval NRF_ERROR_NOT_FOUND Unknown connection handle.
 */
uint32_t ble_hids_boot_kb_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle,
				       const struct ble_hids_boot_keyboard_input_report *report);

/**
 * @brief Send a boot mouse input report.
 *
 * @param hids HID service.
 * @param conn_handle Connection handle.
 * @param report Boot mouse input report.
 *
 * @retval 0 On success.
 * @retval NRF_ERROR_NULL If @p hids or @p report are @c NULL.
 * @retval NRF_ERROR_NOT_FOUND Unknown connection handle.
 */
uint32_t ble_hids_boot_mouse_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle,
					  const struct ble_hids_boot_mouse_input_report *report);

/**
 * @brief Retrieve output report data.
 *
 * @param hids HID service.
 * @param rep_index Output report index.
 * @param len Length of requested data.
 * @param offset Offset of the data in the report.
 * @param conn_handle Connection handle.
 * @param outp_rep Output report buffer.
 *
 * @retval 0 On success.
 * @retval NRF_ERROR_NULL If @p hids or @p outp_rep are @c NULL.
 * @retval NRF_ERROR_NOT_FOUND Unknown connection handle.
 * @retval NRF_ERROR_INVALID_PARAM Report index @p rep_index is invalid.
 * @retval NRF_ERROR_DATA_SIZE The operation exceeds the maximum characteristic length.
 */
uint32_t ble_hids_outp_rep_get(struct ble_hids *hids, uint8_t rep_index, uint16_t len,
			       uint8_t offset, uint16_t conn_handle, uint8_t *outp_rep);

#ifdef __cplusplus
}
#endif

#endif /* BLE_HIDS_H__ */

/** @} */
