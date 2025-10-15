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
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Define a HID service instance.
 *
 * Define a HID service instance and register it as a Bluetooth event observer.
 *
 * @param _name Name of BLE HIDS instance.
 */
#define BLE_HIDS_DEF(_name)                                                                        \
	static struct ble_hids _name = {                                                           \
		.link_ctx_storage =                                                                \
			{                                                                          \
				.max_links_cnt = CONFIG_BLE_HIDS_MAX_CLIENTS,                      \
				.link_ctx_size =                                                   \
					sizeof(uint32_t) * BYTES_TO_WORDS(BLE_HIDS_LINK_CTX_SIZE), \
			},                                                                         \
	};                                                                                         \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_hids_on_ble_evt, &_name, 0)

/**
 * @brief HID boot keyboard input report maximum size, in bytes.
 */
#define BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE	  8
/**
 * @brief HID boot keyboard output report maximum size, in bytes.
 */
#define BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE	  1
/**
 * @brief HID boot mouse input report maximum size, in bytes.
 */
#define BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE 8

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
 * @brief HID input report
 */
struct ble_hids_input_report {
	/**
	 * @brief Index of the characteristic.
	 * Corresponding to the index in struct ble_hids.inp_rep_array as passed to ble_hids_init()
	 */
	uint8_t report_index;
	/**
	 * @brief Data to be sent.
	 */
	uint8_t *data;
	/**
	 * @brief Size of data to be sent.
	 */
	uint16_t len;
};

/**
 * @brief BLE HID service boot keyboard input report.
 */
struct ble_hids_boot_keyboard_input_report {
	/**
	 * @brief Length of boot keyboard input data.
	 */
	uint16_t len;
	/**
	 * @brief Boot keyboard input data.
	 */
	uint8_t *data;
};

/**
 * @brief BLE HID service boot mouse input report.
 */
struct ble_hids_boot_mouse_input_report {
	/**
	 * @brief Buttons mask.
	 */
	uint8_t buttons;
	/**
	 * @brief Horizontal movement.
	 */
	int8_t delta_x;
	/**
	 * @brief Vertical movement.
	 */
	int8_t delta_y;
	/**
	 * @brief Optional data length.
	 */
	uint16_t optional_data_len;
	/**
	 * @brief Optional data.
	 */
	uint8_t optional_data[5];
};

/** @brief HID Host context structure. It keeps information relevant to a single host. */
struct ble_hids_client_context {
	/**
	 * @brief Protocol mode.
	 */
	uint8_t protocol_mode;
	/**
	 * @brief HID Control Point.
	 */
	uint8_t ctrl_pt;
};

#define BLE_HIDS_LINK_CTX_SIZE                                                                     \
	(sizeof(struct ble_hids_client_context) + CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN +           \
	 CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_LEN + CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_LEN +          \
	 (BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE) + (BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE) +    \
	 (BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE))

/** @brief Size of links context memory pool. */
#define CTX_DATA_POOL_SIZE ((CONFIG_BLE_HIDS_MAX_CLIENTS) * BYTES_TO_WORDS(BLE_HIDS_LINK_CTX_SIZE))

struct ble_hids_link_ctx_storage {
	/**
	 * @brief Links context memory pool.
	 */
	uint32_t ctx_data_pool[CTX_DATA_POOL_SIZE];
	/**
	 * @brief Maximum number of concurrent links.
	 */
	uint8_t const max_links_cnt;
	/**
	 * @brief Context size in bytes for a single link (word-aligned).
	 */
	uint16_t const link_ctx_size;
};

/* Forward declaration of ble_hids structure. */
struct ble_hids;

/** @brief HID Service characteristic id. */
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
	uint8_t report_type;
	/**
	 * @brief Index of the characteristic
	 *
	 * Only used when @ref uuid is BLE_UUID_REPORT_CHAR.
	 */
	uint8_t report_index;
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
	 * @brief Notification enabled event.
	 */
	BLE_HIDS_EVT_NOTIF_ENABLED,
	/**
	 * @brief Notification disabled event.
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
	BLE_HIDS_EVT_REPORT_READ,
	/**
	 * @brief Error.
	 */
	BLE_HIDS_EVT_ERROR,
};

/**
 * @brief HID Service event.
 */
struct ble_hids_evt {
	/**
	 * @brief Event type.
	 */
	enum ble_hids_evt_type evt_type;
	/**
	 * @brief BLE event.
	 */
	ble_evt_t const *ble_evt;
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
			uint8_t const *data;
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
		/**
		 * @brief Parameters for @ref BLE_HIDS_EVT_ERROR.
		 */
		struct {
			/**
			 * @brief Error reason.
			 */
			uint32_t reason;
		} error;
	} params;
};

/**
 * @brief HID service event handler type.
 */
typedef void (*ble_hids_evt_handler_t)(struct ble_hids *hids, const struct ble_hids_evt *evt);

/** @brief Security requirements for HID Service characteristic. */
struct ble_hids_char_sec {
	/**
	 * @brief Security requirement for reading HID Service characteristic value.
	 */
	ble_gap_conn_sec_mode_t read;
	/**
	 * @brief Security requirement for writing HID Service characteristic value.
	 */
	ble_gap_conn_sec_mode_t write;
	/**
	 * @brief Security requirement for writing HID Service characteristic CCCD.
	 */
	ble_gap_conn_sec_mode_t cccd_write;
};

/** @brief HID Report configuration. */
struct ble_hids_report_config {
	/**
	 * @brief Maximum length of characteristic value.
	 */
	uint16_t len;
	/**
	 * @brief Non-zero value if there is more than one instance of the same Report Type
	 */
	uint8_t report_id;
	/**
	 * @brief Type of Report characteristic (see @ref BLE_HIDS_REPORT_TYPE)
	 */
	uint8_t report_type;
	/**
	 * @brief Security requirements for HID Service Input Report characteristic.
	 */
	struct ble_hids_char_sec sec;
};

/**
 * @brief HID Service Report Map characteristic init structure. This contains all options and data
 *        needed for initialization of the Report Map characteristic.
 */
struct ble_hids_rep_map_config {
	/**
	 * @brief Report map data.
	 */
	uint8_t *data;
	/**
	 * @brief Length of report map data.
	 */
	uint16_t len;
	/**
	 * @brief Number of Optional External Report Reference descriptors.
	 */
	uint8_t ext_rep_ref_num;
	/**
	 * @brief Optional External Report Reference descriptor (will be added if != NULL).
	 */
	ble_uuid_t const *ext_rep_ref;
	/**
	 * @brief Security requirement for HID Service Report Map characteristic.
	 */
	struct ble_hids_char_sec sec;
};

/** @brief HID Report characteristic structure. */
struct ble_hids_rep_char {
	/**
	 * @brief Handles related to the Report characteristic.
	 */
	ble_gatts_char_handles_t char_handles;
	/**
	 * @brief Handle of the Report Reference descriptor.
	 */
	uint16_t ref_handle;
};

/**
 * @brief HID service configuration.
 */
struct ble_hids_config {
	/**
	 * @brief HID service event handler.
	 */
	ble_hids_evt_handler_t evt_handler;
	/**
	 * @brief Number of Input Report characteristics.
	 */
	uint8_t input_report_count;
	/**
	 * @brief Information about the Input Report characteristics.
	 */
	struct ble_hids_report_config const *input_report;
	/**
	 * @brief Number of Output Report characteristics.
	 */
	uint8_t output_report_count;
	/**
	 * @brief Information about the Output Report characteristics.
	 */
	struct ble_hids_report_config const *output_report;
	/**
	 * @brief Number of Feature Report characteristics.
	 */
	uint8_t feature_report_count;
	/**
	 * @brief Information about the Feature Report characteristics.
	 */
	struct ble_hids_report_config const *feature_report;
	/**
	 * @brief Information nedeed for initialization of the Report Map characteristic.
	 */
	struct ble_hids_rep_map_config report_map;
	/**
	 * @brief Value of the HID Information characteristic.
	 */
	struct {
		/**
		 * @brief 16-bit unsigned integer representing version number of base
		 * USB HID Specification implemented by HID Device
		 */
		uint16_t bcd_hid;
		/**
		 * @brief Identifies which country the hardware is localized for. Most hardware is
		 * not localized and thus this value would be zero (0).
		 */
		uint8_t b_country_code;
		/** @brief HID information flags. */
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
		} flags;
		/**
		 * @brief Security requirement for reading HID Information characteristic value.
		 */
		ble_gap_conn_sec_mode_t rd_sec;
	} hid_information;
	/**
	 * @brief Number of services to include in HID service.
	 */
	uint8_t included_services_count;
	/**
	 * @brief Array of services to include in HID service.
	 */
	uint16_t *included_services_array;
	/**
	 * @brief Security requirement for HID service Protocol Mode characteristic.
	 *
	 * @note Only read and write are used.
	 */
	struct ble_hids_char_sec protocol_mode_sec;
	/**
	 * @brief Security requirement for HID service Control Point characteristic.
	 *
	 * @note Only write is used.
	 */
	struct ble_hids_char_sec ctrl_point_sec;
	/**
	 * @brief Security requirements for HID Boot Keyboard Input Report characteristic.
	 */
	struct ble_hids_char_sec boot_mouse_inp_rep_sec;
	/**
	 * @brief Security requirements for HID Boot Keyboard Input Report characteristic.
	 */
	struct ble_hids_char_sec boot_kb_inp_rep_sec;
	/**
	 * @brief Security requirements for HID Boot Keyboard Output Report characteristic.
	 */
	struct ble_hids_char_sec boot_kb_outp_rep_sec;
};

/** @brief HID Service structure. This contains various status information for the service. */
struct ble_hids {
	/**
	 * @brief Event handler to be called for handling events in the HID Service.
	 */
	ble_hids_evt_handler_t evt_handler;
	/**
	 * @brief Handle of HID Service (as provided by the BLE stack).
	 */
	uint16_t service_handle;
	/**
	 * @brief Handles related to the Protocol Mode characteristic
	 * (will only be created if CONFIG_BLE_HIDS_BOOT_KEYBOARD or CONFIG_BLE_HIDS_BOOT_MOUSE is
	 * set).
	 */
	ble_gatts_char_handles_t protocol_mode_handles;
	/**
	 * @brief Number of Input Report characteristics.
	 */
	uint8_t input_report_count;
	/**
	 * @brief Information about the Input Report characteristics.
	 */
	struct ble_hids_rep_char inp_rep_array[CONFIG_BLE_HIDS_INPUT_REPORT_MAX_NUM];
	/**
	 * @brief Number of Output Report characteristics.
	 */
	uint8_t output_report_count;
	/**
	 * @brief Information about the Output Report characteristics.
	 */
	struct ble_hids_rep_char outp_rep_array[CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_NUM];
	/**
	 * @brief Number of Feature Report characteristics.
	 */
	uint8_t feature_report_count;
	/**
	 * @brief Information about the Feature Report characteristics.
	 */
	struct ble_hids_rep_char feature_rep_array[CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_NUM];
	/**
	 * @brief Handles related to the Report Map characteristic.
	 */
	ble_gatts_char_handles_t rep_map_handles;
	/**
	 * @brief Handle of the Report Map External Report Reference descriptor.
	 */
	uint16_t rep_map_ext_rep_ref_handle;
	/**
	 * @brief Handles related to the Boot Keyboard Input Report characteristic
	 * (will only be created if struct ble_hids_config.is_kb is set).
	 */
	ble_gatts_char_handles_t boot_kb_inp_rep_handles;
	/**
	 * @brief Handles related to the Boot Keyboard Output Report characteristic
	 * (will only be created if struct ble_hids_config.is_kb is set).
	 */
	ble_gatts_char_handles_t boot_kb_outp_rep_handles;
	/**
	 * @brief Handles related to the Boot Mouse Input Report characteristic
	 * (will only be created if struct ble_hids_config.is_mouse is set).
	 */
	ble_gatts_char_handles_t boot_mouse_inp_rep_handles;
	/**
	 * @brief Handles related to the Report Map characteristic.
	 */
	ble_gatts_char_handles_t hid_information_handles;
	/**
	 * @brief Handles related to the Report Map characteristic.
	 */
	ble_gatts_char_handles_t hid_control_point_handles;
	/**
	 * @brief Link context storage with handles of all current connections and its data context.
	 */
	struct ble_hids_link_ctx_storage link_ctx_storage;
	/**
	 * @brief Information about the Input Report characteristics.
	 */
	struct ble_hids_report_config const *inp_rep_init_array;
	/**
	 * @brief Information about the Output Report characteristics.
	 */
	struct ble_hids_report_config const *outp_rep_init_array;
	/**
	 * @brief Information about the Feature Report characteristics.
	 */
	struct ble_hids_report_config const *feature_rep_init_array;
};

/**
 * @brief Function for handling the Application's BLE Stack events.
 *
 * @details Handles all events from the BLE stack of interest to the HID Service.
 *
 * @note This function is registered with a NRF_SDH_BLE_OBSERVER and is called automatically
 *       by the SoftDevice Handler.
 *
 * @param[in] ble_evt Event received from the BLE stack.
 * @param[in] context HID Service structure.
 */
void ble_hids_on_ble_evt(ble_evt_t const *ble_evt, void *context);

/**
 * @brief Function for initializing the HID Service.
 *
 * @param[out] hids HID Service structure. This structure will have to be supplied by the
 *                  application. It will be initialized by this function, and will later be
 *                  used to identify this particular service instance.
 * @param[in] hids_init Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p hids or @p hids_config are @c NULL.
 * @retval NRF_ERROR_INVALID_PARAM Invalid parameters.
 */
uint32_t ble_hids_init(struct ble_hids *hids, const struct ble_hids_config *hids_init);

/**
 * @brief Function for sending Input Report.
 *
 * @details Sends data on an Input Report characteristic.
 *
 * @param[in] hids HID Service structure.
 * @param[in] conn_handle  Connection handle, where the notification will be sent.
 * @param[in] report HID input report.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p hids or @p data are @c NULL.
 * @retval NRF_ERROR_NOT_FOUND Unknown connection handle.
 * @retval NRF_ERROR_INVALID_PARAM Report index @p rep_index is invalid.
 * @retval NRF_ERROR_DATA_SIZE Report data length @p len exceeds maximum characteristic length.
 */
uint32_t ble_hids_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle,
			       struct ble_hids_input_report *report);

/**
 * @brief Function for sending Boot Keyboard Input Report.
 *
 * @details Sends data on an Boot Keyboard Input Report characteristic.
 *
 * @param[in] hids HID Service structure.
 * @param[in] len Length of data to be sent.
 * @param[in] data Data to be sent.
 * @param[in] conn_handle  Connection handle, where the notification will be sent.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p hids or @p report are @c NULL.
 * @retval NRF_ERROR_NOT_FOUND Unknown connection handle.
 */
uint32_t ble_hids_boot_kb_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle,
				       struct ble_hids_boot_keyboard_input_report *report);

/**
 * @brief Function for sending Boot Mouse Input Report.
 *
 * @details Sends data on an Boot Mouse Input Report characteristic.
 *
 * @param[in] hids HID Service structure.
 * @param[in] conn_handle Connection handle.
 * @param[in] report Boot Mouse input report.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p hids or @p report are @c NULL.
 * @retval NRF_ERROR_NOT_FOUND Unknown connection handle.
 */
uint32_t ble_hids_boot_mouse_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle,
					  struct ble_hids_boot_mouse_input_report *report);

/**
 * @brief Function for getting the current value of Output Report from the stack.
 *
 * @details Fetches the current value of the output report characteristic from the stack.
 *
 * @param[in] hids HID Service structure.
 * @param[in] report_index Index of the characteristic (corresponding to the index in
 *                         struct ble_hids.outp_rep_array as passed to ble_hids_init()).
 * @param[in] len Length of output report needed.
 * @param[in] offset Offset in bytes to read from.
 * @param[in] conn_handle Connection handle.
 * @param[out] outp_rep Output report buffer.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p hids or @p outp_rep are @c NULL.
 * @retval NRF_ERROR_NOT_FOUND Unknown connection handle.
 * @retval NRF_ERROR_INVALID_PARAM Report index @p rep_index is invalid.
 * @retval NRF_ERROR_DATA_SIZE The operation exceeds the maximum characteristic length.
 */
uint32_t ble_hids_outp_rep_get(struct ble_hids *hids, uint8_t report_index, uint16_t len,
			       uint8_t offset, uint16_t conn_handle, uint8_t *outp_rep);

#ifdef __cplusplus
}
#endif

#endif /* BLE_HIDS_H__ */

/** @} */
