/**
 * Copyright (c) 2016 - 2026, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bm_buttons.h>
#include <hal/nrf_gpio.h>
#include <board-config.h>
#include "ble.h"
#include "ble_gap.h"
#include "ble_hci.h"
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#include <bm/bluetooth/services/ble_nus_client.h>
#include "ble_gatt.h"
#include <bm/bluetooth/ble_scan.h>

#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#define APP_BLE_CONN_CFG_TAG                                                                       \
	1 /**< Tag that refers to the BLE stack configuration set with @ref sd_ble_cfg_set. The    \
	     default tag is @ref BLE_CONN_CFG_TAG_DEFAULT. */
#define APP_BLE_OBSERVER_PRIO                                                                      \
	3 /**< BLE observer priority of the application. There is no need to modify this value. */

#define UART_TX_BUF_SIZE 256 /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 256 /**< UART RX buffer size. */

#define NUS_SERVICE_UUID_TYPE                                                                      \
	BLE_UUID_TYPE_VENDOR_BEGIN /**< UUID type for the Nordic UART Service (vendor specific).   \
				    */

#define ECHOBACK_BLE_UART_DATA                                                                     \
	1 /**< Echo the UART data that is received over the Nordic UART Service (NUS) back to the  \
	     sender. */

BLE_NUS_C_DEF(m_ble_nus_c);	 /**< BLE Nordic UART Service (NUS) client instance. */
BLE_GATT_DEF(m_gatt);		 /**< GATT module instance. */
BLE_DB_DISCOVERY_DEF(m_db_disc); /**< Database discovery module instance. */
BLE_SCAN_DEF(m_scan);		 /**< Scanning Module instance. */
BLE_GQ_DEF(m_ble_gatt_queue);	 /**< BLE GATT Queue instance. */

LOG_MODULE_REGISTER(app, CONFIG_APP_BLE_NUS_CLIENT_LOG_LEVEL);

static uint16_t m_ble_nus_max_data_len =
	BLE_GATT_ATT_MTU_DEFAULT - OPCODE_LENGTH -
	HANDLE_LENGTH; /**< Maximum length of data (in bytes) that can be transmitted to the peer by
			  the Nordic UART service module. */

/**
 * @brief NUS UUID.
 */
static ble_uuid_t const m_nus_uuid = {.uuid = BLE_UUID_NUS_SERVICE, .type = NUS_SERVICE_UUID_TYPE};

/**
 * @brief Function for handling asserts in the SoftDevice.
 *
 * @details This function is called in case of an assert in the SoftDevice.
 *
 * @warning This handler is only an example and is not meant for the final product. You need to
 * analyze how your product is supposed to react in case of assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num     Line number of the failing assert call.
 * @param[in] file_name  File name of the failing assert call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t *file_name)
{
	app_error_handler(0xDEADBEEF, line_num, file_name);
}

/**
 * @brief Function for handling the Nordic UART Service Client errors.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nus_error_handler(uint32_t nrf_error)
{
	APP_ERROR_HANDLER(nrf_error);
}

/**
 * @brief Function to start scanning.
 */
static void scan_start(void)
{
	uint32_t nrf_err;

	nrf_err = nrf_ble_scan_start(&m_scan);
	if (nrf_err) {
		LOG_ERR("Failed to start scanning, nrf_error %#x", nrf_err);
	}

	// nrf_err = bsp_indication_set(BSP_INDICATE_SCANNING);
	// APP_ERROR_CHECK(ret);
}

/**
 * @brief Function for handling Scanning Module events.
 */
static void scan_evt_handler(scan_evt_t const *scan_evt)
{
	uint32_t nrf_err;

	switch (scan_evt->scan_evt_id) {
	case NRF_BLE_SCAN_EVT_CONNECTING_ERROR: {
		nrf_err = scan_evt->params.connecting_err.err_code;
		if (nrf_err) {
			LOG_ERR("Failed to connect, nrf_error %#x", nrf_err);
		}
	} break;

	case NRF_BLE_SCAN_EVT_CONNECTED: {
		ble_gap_evt_connected_t const *connected = scan_evt->params.connected.connected;
		// Scan is automatically stopped by the connection.
		LOG_INF("Connecting to target %02x%02x%02x%02x%02x%02x",
			connected->peer_addr.addr[0], connected->peer_addr.addr[1],
			connected->peer_addr.addr[2], connected->peer_addr.addr[3],
			connected->peer_addr.addr[4], connected->peer_addr.addr[5]);
	} break;

	case NRF_BLE_SCAN_EVT_SCAN_TIMEOUT: {
		LOG_INF("Scan timed out.");
		scan_start();
	} break;

	default:
		break;
	}
}

/**
 * @brief Function for initializing the scanning and setting the filters.
 */
static void scan_init(void)
{
	uint32_t nrf_err;
	struct ble_scan_config init_scan = {0};

	init_scan.connect_if_match = true;
	init_scan.conn_cfg_tag = APP_BLE_CONN_CFG_TAG;

	nrf_err = nrf_ble_scan_init(&m_scan, &init_scan, scan_evt_handler);
	if (nrf_err) {
		LOG_ERR("Failed to initialize scanning, nrf_error %#x", nrf_err);
	}
	nrf_err = nrf_ble_scan_filter_set(&m_scan, SCAN_UUID_FILTER, &m_nus_uuid);
	if (nrf_err) {
		LOG_ERR("Failed to set filter, nrf_error %#x", nrf_err);
	}
	nrf_err = nrf_ble_scan_filters_enable(&m_scan, NRF_BLE_SCAN_UUID_FILTER, false);
	if (nrf_err) {
		LOG_ERR("Enabling filter failed, nrf_error %#x", nrf_err);
	}
}

/**
 * @brief Function for handling database discovery events.
 *
 * @details This function is a callback function to handle events from the database discovery
 * module. Depending on the UUIDs that are discovered, this function forwards the events to their
 * respective services.
 *
 * @param[in] event  Pointer to the database discovery event.
 */
static void db_disc_handler(struct ble_db_discovery_evt *evt)
{
	ble_nus_c_on_db_disc_evt(&m_ble_nus_c, evt);
}

/**
 * @brief Function for handling characters received by the Nordic UART Service (NUS).
 *
 * @details This function takes a list of characters of length data_len and prints the characters
 * out on UART. If @ref ECHOBACK_BLE_UART_DATA is set, the data is sent back to sender.
 */
static void ble_nus_chars_received_uart_print(uint8_t *data, uint16_t data_len)
{
	uint32_t ret_val;

	LOG_DBG("Receiving data.");
	LOG_HEXDUMP_DBG(data, data_len);

	for (uint32_t i = 0; i < data_len; i++) {
		do {
			ret_val = app_uart_put(data[i]);
			if ((ret_val != NRF_SUCCESS) && (ret_val != NRF_ERROR_BUSY)) {
				LOG_ERR("app_uart_put failed for index 0x%04x.", i);
			}
		} while (ret_val == NRF_ERROR_BUSY);
	}
	if (data[data_len - 1] == '\r') {
		while (app_uart_put('\n') == NRF_ERROR_BUSY)
			;
	}
	if (ECHOBACK_BLE_UART_DATA) {
		// Send data back to the peripheral.
		do {
			ret_val = ble_nus_c_string_send(&m_ble_nus_c, data, data_len);
			if ((ret_val != NRF_SUCCESS) && (ret_val != NRF_ERROR_BUSY)) {
				LOG_ERR("Failed sending NUS message. Error 0x%x. ", ret_val);
			}
		} while (ret_val == NRF_ERROR_BUSY);
	}
}

/**
 * @brief   Function for handling app_uart events.
 *
 * @details This function receives a single character from the app_uart module and appends it to
 *          a string. The string is sent over BLE when the last character received is a
 *          'new line' '\n' (hex 0x0A) or if the string reaches the maximum data length.
 */
//void uart_event_handle(app_uart_evt_t *event)
//{
//	static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
//	static uint16_t index = 0;
//	uint32_t nrf_err;
//
//	switch (event->evt_type) {
//	/**@snippet [Handling data from UART] */
//	case APP_UART_DATA_READY:
//		(void)app_uart_get(&data_array[index]);
//		index++;
//
//		if ((data_array[index - 1] == '\n') || (data_array[index - 1] == '\r') ||
//		    (index >= (m_ble_nus_max_data_len))) {
//			LOG_DBG("Ready to send data over BLE NUS");
//			LOG_HEXDUMP_DBG(data_array, index);
//
//			do {
//				nrf_err = ble_nus_c_string_send(&m_ble_nus_c, data_array, index);
//				if ((nrf_err != NRF_ERROR_INVALID_STATE) &&
//				    (nrf_err != NRF_ERROR_RESOURCES)) {
//					if (nrf_err) {
//						LOG_ERR("Failed to send string, nrf_error %#x",
//							nrf_err);
//					}
//				}
//			} while (nrf_err == NRF_ERROR_RESOURCES);
//
//			index = 0;
//		}
//		break;
//
//	/**@snippet [Handling data from UART] */
//	case APP_UART_COMMUNICATION_ERROR:
//		LOG_ERR("Communication error occurred while handling UART.");
//		APP_ERROR_HANDLER(event->data.error_communication);
//		break;
//
//	case APP_UART_FIFO_ERROR:
//		LOG_ERR("Error occurred in FIFO module used by UART.");
//		APP_ERROR_HANDLER(event->data.error_code);
//		break;
//
//	default:
//		break;
//	}
//}

/**@brief Callback handling Nordic UART Service (NUS) client events.
 *
 * @details This function is called to notify the application of NUS client events.
 *
 * @param[in]   ble_nus_c   NUS client handle. This identifies the NUS client.
 * @param[in]   ble_nus_evt Pointer to the NUS client event.
 */

/**@snippet [Handling events from the ble_nus_c module] */
static void ble_nus_c_evt_handler(struct ble_nus_c *ble_nus_c,
				  struct ble_nus_c_evt const *ble_nus_evt)
{
	uint32_t nrf_err;

	switch (ble_nus_evt->evt_type) {
	case BLE_NUS_C_EVT_DISCOVERY_COMPLETE:
		LOG_INF("Discovery complete.");
		nrf_err = ble_nus_c_handles_assign(ble_nus_c, ble_nus_evt->conn_handle,
						   &ble_nus_evt->handles);
		if (nrf_err) {
			LOG_ERR("Failed to assign handles, nrf_error %#x", nrf_err);
		}

		nrf_err = ble_nus_c_tx_notif_enable(ble_nus_c);
		if (nrf_err) {
			LOF_ERR("Failed to enable peer tx notifications, nrf_error %#x", nrf_err);
		}
		LOG_INF("Connected to device with Nordic UART Service.");
		break;

	case BLE_NUS_C_EVT_NUS_TX_EVT:
		ble_nus_chars_received_uart_print(ble_nus_evt->data, ble_nus_evt->data_len);
		break;

	case BLE_NUS_C_EVT_DISCONNECTED:
		LOG_INF("Disconnected.");
		scan_start();
		break;
	}
}
/**@snippet [Handling events from the ble_nus_c module] */

/**
 * @brief Function for handling shutdown events.
 *
 * @param[in]   event       Shutdown type.
 */
// static bool shutdown_handler(nrf_pwr_mgmt_evt_t event)
//{
//	ret_code_t err_code;
//
//	err_code = bsp_indication_set(BSP_INDICATE_IDLE);
//	APP_ERROR_CHECK(err_code);
//
//	switch (event) {
//	case NRF_PWR_MGMT_EVT_PREPARE_WAKEUP:
//		// Prepare wakeup buttons.
//		err_code = bsp_btn_ble_sleep_mode_prepare();
//		APP_ERROR_CHECK(err_code);
//		break;
//
//	default:
//		break;
//	}
//
//	return true;
// }

// NRF_PWR_MGMT_HANDLER_REGISTER(shutdown_handler, APP_SHUTDOWN_HANDLER_PRIORITY);

/**
 * @brief Function for handling BLE events.
 *
 * @param[in]   ble_evt   Bluetooth stack event.
 * @param[in]   context   Unused.
 */
static void ble_evt_handler(ble_evt_t const *ble_evt, void *context)
{
	uint32_t err_code;
	ble_gap_evt_t const *gap_evt = &ble_evt->evt.gap_evt;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		err_code = ble_nus_c_handles_assign(&m_ble_nus_c, ble_evt->evt.gap_evt.conn_handle,
						    NULL);
		if (err_code) {
			LOG_ERR("Failed to assign handles, nrf_error %#x", err_code);
		}

		// err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
		// APP_ERROR_CHECK(err_code);

		// start discovery of services. The NUS Client waits for a discovery result
		err_code = ble_db_discovery_start(&m_db_disc, ble_evt->evt.gap_evt.conn_handle);
		if (err_code) {
			LOG_ERR("Failed to start db discovery, nrf_error %#x", err_code);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:

		LOG_INF("Disconnected. conn_handle: 0x%x, reason: 0x%x", gap_evt->conn_handle,
			gap_evt->params.disconnected.reason);
		break;

	case BLE_GAP_EVT_TIMEOUT:
		if (gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN) {
			LOG_INF("Connection Request timed out.");
		}
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		// Pairing not supported.
		err_code = sd_ble_gap_sec_params_reply(ble_evt->evt.gap_evt.conn_handle,
						       BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL,
						       NULL);
		if (err_code) {
			LOG_ERR("gap_sec_params_reply failed, nrf_error %#x", err_code);
		}
		break;

	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
		// Accepting parameters requested by peer.
		err_code = sd_ble_gap_conn_param_update(
			gap_evt->conn_handle,
			&gap_evt->params.conn_param_update_request.conn_params);
		if (err_code) {
			LOG_ERR("gap_conn_param_update failed, nrf_error %#x", err_code);
		}
		break;

	case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
		LOG_DBG("PHY update request.");
		ble_gap_phys_t const phys = {
			.rx_phys = BLE_GAP_PHY_AUTO,
			.tx_phys = BLE_GAP_PHY_AUTO,
		};
		err_code = sd_ble_gap_phy_update(ble_evt->evt.gap_evt.conn_handle, &phys);
		if (err_code) {
			LOG_ERR("gap_phy_update failed, nrf_error %#x", err_code);
		}
	} break;

	case BLE_GATTC_EVT_TIMEOUT:
		// Disconnect on GATT Client timeout event.
		LOG_DBG("GATT Client Timeout.");
		err_code = sd_ble_gap_disconnect(ble_evt->evt.gattc_evt.conn_handle,
						 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (err_code) {
			LOG_ERR("gap_disconnect failed, nrf_error %#x", err_code);
		}
		break;

	case BLE_GATTS_EVT_TIMEOUT:
		// Disconnect on GATT Server timeout event.
		LOG_DBG("GATT Server Timeout.");
		err_code = sd_ble_gap_disconnect(ble_evt->evt.gatts_evt.conn_handle,
						 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (err_code) {
			LOG_ERR("gap_disconnect failed, nrf_error %#x", err_code);
		}
		break;

	default:
		break;
	}
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
	uint32_t err_code;

	err_code = nrf_sdh_enable_request();
	if (err_code) {
		LOG_ERR("sdh_enable_request failed, nrf_error %#x", err_code);
	}

	// Configure the BLE stack using the default settings.
	// Fetch the start address of the application RAM.
	uint32_t ram_start = 0;
	err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
	if (err_code) {
		LOG_ERR("sdh_cfg_set failed, nrf_error %#x", err_code);
	}

	// Enable BLE stack.
	err_code = nrf_sdh_ble_enable(&ram_start);
	if (err_code) {
		LOG_ERR("sdh_enable_enable failed, nrf_error %#x", err_code);
	}

	// Register a handler for BLE events.
	NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

/**
 * @brief Function for handling events from the GATT library.
 */
void gatt_evt_handler(struct ble_gatt *gatt, struct ble_gatt_evt const *evt)
{
	if (evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED) {
		LOG_INF("ATT MTU exchange completed.");

		m_ble_nus_max_data_len =
			evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
		LOG_INF("Ble NUS max data length set to 0x%X(%d)", m_ble_nus_max_data_len,
			m_ble_nus_max_data_len);
	}
}

/**
 * @brief Function for initializing the GATT library.
 */
void gatt_init(void)
{
	// uint32_t err_code;
	//
	// err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
	// APP_ERROR_CHECK(err_code);
	//
	////ble_conn_params_att_mtu_set()
	// err_code = nrf_ble_gatt_att_mtu_central_set(&m_gatt, BLE_GATT_MAX_MTU_SIZE);
	//
	// APP_ERROR_CHECK(err_code);
}

static void button_disconnect_handler()
{
}

static void button_sleep_handler()
{
}

/**
 * @brief Function for initializing the UART.
 */
// static void uart_init(void) // ble_nus peripheral uses uarte_init function, change?
//{
//	uint32_t err_code;
//
//	app_uart_comm_params_t const comm_params = {.rx_pin_no = RX_PIN_NUMBER,
//						    .tx_pin_no = TX_PIN_NUMBER,
//						    .rts_pin_no = RTS_PIN_NUMBER,
//						    .cts_pin_no = CTS_PIN_NUMBER,
//						    .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
//						    .use_parity = false,
//						    .baud_rate = UART_BAUDRATE_BAUDRATE_Baud115200};
//
//	APP_UART_FIFO_INIT(&comm_params, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, uart_event_handle,
//			   IRQ_PRIORITY_LOWEST, err_code);
//
//	APP_ERROR_CHECK(err_code);
// }

/**
 * @brief Function for initializing the Nordic UART Service (NUS) client.
 */
static void nus_c_init(void)
{
	uint32_t err_code;

	struct ble_nus_c_init init;

	init.evt_handler = ble_nus_c_evt_handler;
	init.error_handler = nus_error_handler;
	init.gatt_queue = &m_ble_gatt_queue;

	err_code = ble_nus_c_init(&m_ble_nus_c, &init);
	if (err_code) {
		LOG_ERR("Failed to initialize NUS, nrf_error %#x", err_code);
	}
}

/**
 * @brief Function for initializing buttons and leds.
 *
 * @param[out] erase_bonds true if the clear bonding button was pressed to wake the application up.
 */
static int buttons_leds_init(void)
{
	int err;

	static struct bm_buttons_config btn_configs[4] = {
		{
			.pin_number = BOARD_PIN_LED_0,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_sleep_handler,
		},
		{
			.pin_number = BOARD_PIN_LED_1,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_disconnect_handler,
		},
	};

	err = bm_buttons_init(btn_configs, ARRAY_SIZE(btn_configs),
			      BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("bm_buttons_init error: %d", err);
		return err;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("bm_buttons_enable error: %d", err);
		return err;
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);

	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_2, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_3, !BOARD_LED_ACTIVE_STATE);

	return 0;
}

/**
 * @brief Function for initializing the timer.
 */
static void timer_init(void)
{
	// uint32_t err_code;
	//
	// err_code = app_timer_init();
	// if (err_code) {
	//	LOG_ERR("Failed to enable app timer, nrf_error %#x", err_code);
	//}
}

/**
 * @brief Function for initializing power management.
 */
static void power_management_init(void)
{
	// uint32_t err_code;
	//
	// err_code = nrf_pwr_mgmt_init();
	// if (err_code) {
	//	LOG_ERR("Failed to enable power management, nrf_error %#x", err_code);
	//}
}

/**
 * @brief Function for initializing the database discovery module.
 */
static void db_discovery_init(void)
{
	struct ble_db_discovery db_init = {0};

	db_init.evt_handler = db_disc_handler;
	db_init.gatt_queue = &m_ble_gatt_queue;

	uint32_t err_code = ble_db_discovery_init(&db_init, NULL);
	if (err_code) {
		LOG_ERR("Failed to enable db discovery, nrf_error %#x", err_code);
	}
}

/**
 * @brief Function for handling the idle state (main loop).
 *
 * @details Handles any pending log operations, then sleeps until the next event occurs.
 */
static void idle_state_handle(void)
{
	// if (NRF_LOG_PROCESS() == false) {
	//	nrf_pwr_mgmt_run();
	// }
}

int main(void)
{
	// Initialize.
	timer_init();
	// uart_init();
	buttons_leds_init();
	db_discovery_init();
	power_management_init();
	ble_stack_init();
	gatt_init();
	nus_c_init();
	scan_init();

	// Start execution.
	printf("BLE UART central example started.\r\n");
	LOG_INF("BLE UART central example started.");
	scan_start();

	// Enter main loop.
	for (;;) {
		idle_state_handle();
	}
}
