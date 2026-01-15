/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <nfc_t4t_lib.h>
#include <nfc/t4t/ndef_file.h>

#include <nfc/ndef/msg.h>
#include <nfc/ndef/ch.h>
#include <bm/nfc/ndef/ch_msg.h>
#include <bm/nfc/ndef/le_oob_rec.h>

#include <nrf_soc.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/services/ble_dis.h>
#include <bm/bluetooth/peer_manager/nrf_ble_lesc.h>
#include <bm/bluetooth/peer_manager/peer_manager.h>
#include <bm/bluetooth/peer_manager/peer_manager_handler.h>

#include <bm/bm_buttons.h>
#include <hal/nrf_gpio.h>
#include <board-config.h>

LOG_MODULE_REGISTER(app, CONFIG_APP_BLE_PERIPHERAL_NFC_PAIRING_LOG_LEVEL);

#define MAX_REC_COUNT		3
#define NDEF_MSG_BUF_SIZE	256

#define NFC_FIELD_LED		BOARD_PIN_LED_1
#define CON_STATUS_LED		BOARD_PIN_LED_0

#define BUTTON_BOND_REMOVE_PIN	BOARD_PIN_BTN_3

/* Perform bonding. */
#define SEC_PARAM_BOND 1
/* Man In The Middle protection not required. */
#define SEC_PARAM_MITM 0
/* LE Secure Connections enabled. */
#define SEC_PARAM_LESC 1
/* Keypress notifications not enabled. */
#define SEC_PARAM_KEYPRESS 0
/* No I/O capabilities. */
#define SEC_PARAM_IO_CAPABILITIES BLE_GAP_IO_CAPS_NONE
/* Out Of Band data not available. */
#define SEC_PARAM_OOB 0
/* Minimum encryption key size. */
#define SEC_PARAM_MIN_KEY_SIZE 7
/* Maximum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE 16

/* BLE Advertising library instance */
BLE_ADV_DEF(ble_adv);

/* BLE Connection handle */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
/* Peer ID */
static uint16_t peer_id = PM_PEER_ID_INVALID;

static ble_gap_lesc_oob_data_t *oob_local;
static uint8_t conn_cnt;
static uint8_t tk_value[NFC_NDEF_LE_OOB_REC_TK_LEN];
static const char device_name[] = CONFIG_BLE_ADV_NAME;

/* Buffer used to hold an NFC NDEF message. */
static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];


static void led_init(void)
{
	nrf_gpio_cfg_output(NFC_FIELD_LED);
	nrf_gpio_cfg_output(CON_STATUS_LED);
}

static void nfc_field_led_on(void)
{
	nrf_gpio_pin_write(NFC_FIELD_LED, BOARD_LED_ACTIVE_STATE);
}

static void nfc_field_led_off(void)
{
	nrf_gpio_pin_write(NFC_FIELD_LED, !BOARD_LED_ACTIVE_STATE);
}

static void con_status_led_on(void)
{
	nrf_gpio_pin_write(CON_STATUS_LED, BOARD_LED_ACTIVE_STATE);
}

static void con_status_led_off(void)
{
	nrf_gpio_pin_write(CON_STATUS_LED, !BOARD_LED_ACTIVE_STATE);
}

static int tk_value_generate(void)
{
	uint32_t nrf_err = sd_rand_application_vector_get(tk_value, sizeof(tk_value));

	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Random TK value generation failed: %d\n", nrf_err);
		return -EFAULT;
	}

	return 0;
}

static uint32_t paring_key_generate(void)
{
	uint32_t nrf_err;

	LOG_INF("Generating new pairing keys");

	nrf_err = nrf_ble_lesc_keypair_generate();

	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Error while generating LESC key pair: %d\n", nrf_err);
		return nrf_err;
	}

	nrf_err = nrf_ble_lesc_own_oob_data_generate();
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Error while generating LESC own OOB data: %d\n", nrf_err);
		return nrf_err;
	}

	oob_local = nrf_ble_lesc_own_oob_data_get();

	return tk_value_generate();
}

static void allow_list_set(enum pm_peer_id_list_skip skip)
{
	uint32_t nrf_err;
	uint16_t peer_ids[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t peer_id_count = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	nrf_err = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
	if (nrf_err) {
		LOG_ERR("Failed to get peer id list, nrf_error %#x", nrf_err);
	}

	LOG_INF("Number of peers added to the allow list: %d, max %d",
		peer_id_count, BLE_GAP_WHITELIST_ADDR_MAX_COUNT);

	nrf_err = pm_allow_list_set(peer_ids, peer_id_count);
	if (nrf_err) {
		LOG_ERR("Failed to set allow list, nrf_error %#x", nrf_err);
	}
}

static void identities_set(enum pm_peer_id_list_skip skip)
{
	uint32_t nrf_err;
	uint16_t peer_ids[BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT];
	uint32_t peer_id_count = BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT;

	nrf_err = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
	if (nrf_err) {
		LOG_ERR("Failed to get peer id list, nrf_error %#x", nrf_err);
	}

	nrf_err = pm_device_identities_list_set(peer_ids, peer_id_count);
	if (nrf_err) {
		LOG_ERR("Failed to set peer manager identity list, nrf_error %#x", nrf_err);
	}
}

static void delete_bonds(void)
{
	uint32_t nrf_err;

	LOG_INF("Erasing bonds");

	nrf_err = pm_peers_delete();
	if (nrf_err) {
		LOG_ERR("Failed to delete peers, nrf_error %#x", nrf_err);
	}
}

static void advertising_start(void)
{
	allow_list_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);

	uint32_t nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_DIRECTED);

	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
	}
}

static void pm_evt_handler(const struct pm_evt *evt)
{
	pm_handler_on_pm_evt(evt);
	pm_handler_disconnect_on_sec_failure(evt);
	pm_handler_flash_clean(evt);

	switch (evt->evt_id) {
	case PM_EVT_CONN_SEC_SUCCEEDED:
		peer_id = evt->peer_id;
		break;

	case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
		if (evt->params.peer_data_update_succeeded.flash_changed &&
		    (evt->params.peer_data_update_succeeded.data_id == PM_PEER_DATA_ID_BONDING)) {
			LOG_INF("New bond, add the peer to the allow list if possible");
			/* Note: You should check on what kind of allow list policy your
			 * application should use.
			 */
			allow_list_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
		}
		break;

	default:
		break;
	}
}

static uint32_t peer_manager_init(void)
{
	ble_gap_sec_params_t sec_param;
	uint32_t nrf_err;

	nrf_err = pm_init();
	if (nrf_err) {
		return nrf_err;
	}

	/* Security parameters to be used for all security procedures. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = SEC_PARAM_BOND,
		.mitm = SEC_PARAM_MITM,
		.lesc = SEC_PARAM_LESC,
		.keypress = SEC_PARAM_KEYPRESS,
		.io_caps = SEC_PARAM_IO_CAPABILITIES,
		.oob = SEC_PARAM_OOB,
		.min_key_size = SEC_PARAM_MIN_KEY_SIZE,
		.max_key_size = SEC_PARAM_MAX_KEY_SIZE,
		.kdist_own.enc = 1,
		.kdist_own.id = 1,
		.kdist_peer.enc = 1,
		.kdist_peer.id = 1,
	};

	nrf_err = pm_sec_params_set(&sec_param);
	if (nrf_err) {
		LOG_ERR("pm_sec_params_set() failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = pm_register(pm_evt_handler);
	if (nrf_err) {
		LOG_ERR("pm_register() failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;

		conn_cnt++;
		con_status_led_on();
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected, reason %d",
			evt->evt.gap_evt.params.disconnected.reason);

		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}

		conn_cnt--;
		if (conn_cnt == 0) {
			con_status_led_off();
		}
		break;

	case BLE_GAP_EVT_PASSKEY_DISPLAY:
		LOG_INF("Passkey: %.*s", BLE_GAP_PASSKEY_LEN,
			evt->evt.gap_evt.params.passkey_display.passkey);
		if (evt->evt.gap_evt.params.passkey_display.match_request) {
			LOG_INF("Pairing request.");
		}
		break;

	case BLE_GAP_EVT_AUTH_KEY_REQUEST:
		LOG_INF("Pairing request.");
		break;

	default:
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static void ble_adv_evt_handler(struct ble_adv *ble_adv, const struct ble_adv_evt *evt)
{
	uint32_t nrf_err;
	ble_gap_addr_t *peer_addr;
	ble_gap_addr_t allow_list_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	ble_gap_irk_t allow_list_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
	uint32_t irk_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	switch (evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("Advertising error %#x", evt->error.reason);
		break;
	case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
	case BLE_ADV_EVT_DIRECTED:
	case BLE_ADV_EVT_FAST:
	case BLE_ADV_EVT_SLOW:
	case BLE_ADV_EVT_FAST_ALLOW_LIST:
	case BLE_ADV_EVT_SLOW_ALLOW_LIST:
	case BLE_ADV_EVT_IDLE:
		break;
	case BLE_ADV_EVT_ALLOW_LIST_REQUEST:
		nrf_err = pm_allow_list_get(allow_list_addrs, &addr_cnt, allow_list_irks, &irk_cnt);
		if (nrf_err) {
			LOG_ERR("Failed to get allow list, nrf_error %#x", nrf_err);
		}
		LOG_DBG("pm_allow_list_get returns %d addr in allow list and %d irk allow list",
				addr_cnt, irk_cnt);

		/* Set the correct identities list
		 * (no excluding peers with no Central Address Resolution).
		 */
		identities_set(PM_PEER_ID_LIST_SKIP_NO_IRK);

		nrf_err = ble_adv_allow_list_reply(ble_adv, allow_list_addrs, addr_cnt,
						   allow_list_irks, irk_cnt);
		if (nrf_err) {
			LOG_ERR("Failed to set allow list, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_ADV_EVT_PEER_ADDR_REQUEST:
		struct pm_peer_data_bonding peer_bonding_data;

		/* Only Give peer address if we have a handle to the bonded peer. */
		if (peer_id != PM_PEER_ID_INVALID) {
			nrf_err = pm_peer_data_bonding_load(peer_id, &peer_bonding_data);
			if (nrf_err != NRF_ERROR_NOT_FOUND) {
				if (nrf_err) {
					LOG_ERR("Failed to load bonding data, nrf_error %#x",
						nrf_err);
				}

				/* Manipulate identities to exclude peers with no
				 * Central Address Resolution.
				 */
				identities_set(PM_PEER_ID_LIST_SKIP_ALL);

				peer_addr = &(peer_bonding_data.peer_ble_id.id_addr_info);
				nrf_err = ble_adv_peer_addr_reply(ble_adv, peer_addr);
				if (nrf_err) {
					LOG_ERR("Failed to reply peer address, nrf_error %#x",
						nrf_err);
				}
			}
		}
		break;
	default:
		break;
	}
}

static void nfc_callback(void *context,
			 nfc_t4t_event_t event,
			 const uint8_t *data,
			 size_t data_length,
			 uint32_t flags)
{
	ARG_UNUSED(context);
	ARG_UNUSED(data);
	ARG_UNUSED(data_length);
	ARG_UNUSED(flags);

	switch (event) {
	case NFC_T4T_EVENT_FIELD_ON:
		nfc_field_led_on();
		break;
	case NFC_T4T_EVENT_FIELD_OFF:
		nfc_field_led_off();
		break;
	case NFC_T4T_EVENT_NDEF_READ:
		advertising_start();
		break;
	default:
		break;
	}
}

static int pairing_msg_generate(uint32_t *len)
{
	int err;
	struct nfc_ndef_le_oob_rec_payload_desc rec_payload;
	struct nfc_ndef_ch_msg_records ch_records;
	uint32_t ndef_size = nfc_t4t_ndef_file_msg_size_get(*len);

	NFC_NDEF_MSG_DEF(hs_msg, 2);

	oob_local = nrf_ble_lesc_own_oob_data_get();
	if (oob_local == NULL) {
		LOG_ERR("Failed to get LESC own OOB data!");
		return -EFAULT;
	}

	err = tk_value_generate();
	if (err) {
		return err;
	}

	memset(&rec_payload, 0, sizeof(rec_payload));

	rec_payload.addr = &oob_local->addr;
	rec_payload.le_sc_data = oob_local;
	rec_payload.tk_value = tk_value;
	rec_payload.local_name = device_name;
	rec_payload.le_role = NFC_NDEF_LE_OOB_REC_LE_ROLE(
		NFC_NDEF_LE_OOB_REC_LE_ROLE_PERIPH_ONLY);
	rec_payload.flags = NFC_NDEF_LE_OOB_REC_FLAGS(BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED);

	NFC_NDEF_LE_OOB_RECORD_DESC_DEF(oob_rec, '0', &rec_payload);
	NFC_NDEF_CH_AC_RECORD_DESC_DEF(oob_ac, NFC_AC_CPS_ACTIVE, 1, "0", 0);
	NFC_NDEF_CH_HS_RECORD_DESC_DEF(hs_rec, NFC_NDEF_CH_MSG_MAJOR_VER,
				       NFC_NDEF_CH_MSG_MINOR_VER, 1);

	ch_records.ac = &NFC_NDEF_CH_AC_RECORD_DESC(oob_ac);
	ch_records.carrier = &NFC_NDEF_LE_OOB_RECORD_DESC(oob_rec);
	ch_records.cnt = 1;

	err = nfc_ndef_ch_msg_hs_create(&NFC_NDEF_MSG(hs_msg),
					&NFC_NDEF_CH_RECORD_DESC(hs_rec),
					&ch_records);
	if (err) {
		return err;
	}

	err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(hs_msg),
				  nfc_t4t_ndef_file_msg_get(ndef_msg_buf),
				  &ndef_size);
	if (err) {
		return err;
	}

	err = nfc_t4t_ndef_file_encode(ndef_msg_buf, &ndef_size);
	if (err) {
		return err;
	}

	*len = ndef_size;

	return 0;
}

static int nfc_init(void)
{
	int err = 0;
	uint32_t len = sizeof(ndef_msg_buf);

	/* Set up NFC */
	err = nfc_t4t_setup(nfc_callback, NULL);
	if (err) {
		LOG_ERR("Cannot setup NFC T4T library!");
		return err;
	}

	/* Prepare pairing message */
	err = pairing_msg_generate(&len);
	if (err) {
		LOG_ERR("Cannot encode pairing message!");
		return err;
	}

	/* Set created message as the NFC payload */
	err = nfc_t4t_ndef_staticpayload_set(ndef_msg_buf, len);
	if (err) {
		LOG_ERR("Cannot set payload!");
		return err;
	}

	/* Start sensing NFC field */
	err = nfc_t4t_emulation_start();
	if (err) {
		LOG_ERR("Cannot start emulation!");
		return err;
	}
	LOG_INF("NFC configuration done");

	return err;
}

static void bm_button_handler(uint8_t pin, uint8_t action)
{
	if (action == BM_BUTTONS_PRESS) {
		switch (pin) {
		case BUTTON_BOND_REMOVE_PIN:
			delete_bonds();
			break;

		default:
			break;
		}
	}
}

static int board_buttons_init(void)
{
	int err;

	static const struct bm_buttons_config configs[4] = {
		{
			.pin_number = BOARD_PIN_BTN_0,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = bm_button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_1,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = bm_button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_2,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = bm_button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_3,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = bm_button_handler,
		},
	};

	err = bm_buttons_init(configs, ARRAY_SIZE(configs), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		return err;
	}

	err = bm_buttons_enable();

	return err;
}

int main(void)
{
	int err;
	uint32_t nrf_err;
	struct ble_dis_config dis_config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
	};

	struct ble_adv_config ble_adv_cfg = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,

		},
	};

	LOG_INF("Starting Peripheral NFC Pairing sample");

	/* Configure LED-pins as outputs */
	led_init();

	err = board_buttons_init();
	if (err) {
		LOG_ERR("Buttons init error %d", err);
		goto fail;
	}

	err = nrf_sdh_enable_request();
	if (err) {
		LOG_ERR("Failed to enable SoftDevice, err %d", err);
		goto fail;
	}

	LOG_INF("SoftDevice enabled");

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		LOG_ERR("Failed to enable BLE, err %d", err);
		goto fail;
	}

	LOG_INF("Bluetooth is enabled!");

	nrf_err = peer_manager_init();
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to initialize Peer Manager, nrf_error %x", nrf_err);
		goto fail;
	}

	nrf_err = ble_dis_init(&dis_config);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to initialize device information service, nrf_error %#x", nrf_err);
		goto fail;
	}

	nrf_err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to initialize BLE advertising, nrf_error %#x", nrf_err);
		goto fail;
	}

	nrf_err = paring_key_generate();
	if (nrf_err != NRF_SUCCESS) {
		goto fail;
	}

	err = nfc_init();
	if (err) {
		goto fail;
	}

	peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
	if (peer_id != PM_PEER_ID_INVALID) {
		LOG_INF("Found existing bond for peer id %d, starting advertising", peer_id);
		advertising_start();
	} else {
		LOG_INF("No existing bonds found, waiting for NFC field to start advertising");
	}

fail:
	/* Main loop */
	while (true) {
		(void)nrf_ble_lesc_request_handler();
		log_flush();
		k_cpu_idle();
	}

	return 0;
}
