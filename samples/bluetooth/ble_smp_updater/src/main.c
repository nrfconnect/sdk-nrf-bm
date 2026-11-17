/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* BLE single-bank updater: serves SMP over an unencrypted BLE link and writes a plain
 * MCUboot image in place into the update slot. This file holds everything it asks of
 * the BLE stack, through the S115 SoftDevice API; the SMP server and the update
 * behind it see only SMP packets. It is the bare-metal form of IronSide DFU's BLE
 * alternate image, and keeps that image's BLE parameters.
 */

/*
 * Fixed parameters. They state what the BLE stack has to support, so a minimal
 * stack can be scoped from this list and the calls below.
 *
 * Link layer and GAP:
 * - Peripheral role only, one connection, one advertising set. 1M PHY only: a PHY
 *   update request is answered with 1M for both directions.
 * - Legacy connectable and scannable undirected advertising, every 100 ms on
 *   channels 37 to 39, with no timeout, filter policy "any": every central may
 *   connect. Advertising data: flags (LE General Discoverable, BR/EDR not
 *   supported) and the complete 128-bit SMP service UUID. Scan response: the
 *   complete local name "IronSide DFU", also the read-only GAP device name.
 * - The device address and the connection parameters are the stack's and the
 *   central's. No Peripheral Preferred Connection Parameters and no Central
 *   Address Resolution characteristic, no appearance.
 * - Connection event length 7.5 ms. Data length: the stack's maximum, on request.
 * - No security: pairing requests are refused with "pairing not supported", a
 *   request to encrypt with a stored key is answered with no key, and every
 *   attribute is open. Nothing is bonded or stored.
 *
 * GATT:
 * - ATT MTU 247. No Service Changed characteristic.
 * - One primary service 8D53DC1D-1DB7-4CD3-868B-8A527460AA84 with one
 *   characteristic DA2E7828-FBCE-4E01-AE9E-261174997C48: write without response
 *   and notify, value up to 244 bytes in this image's RAM, CCCD open. Two vendor
 *   UUID bases, an attribute table of 248 bytes, one queued notification.
 *
 * SMP transport:
 * - A request may span several ATT writes: they are joined until the length in the
 *   SMP header is in, up to SMP_REQ_MAX bytes. A request that outgrows it, or a
 *   write past its end, is dropped. One request in flight.
 * - Each response fits one notification.
 * - A disconnect ends the update session, so the host restarts the upload from
 *   offset 0. A reset command resets once its response is out.
 *
 * Stack placement and clocks:
 * - The stack sits at softdevice_partition and uses the RAM below the chosen
 *   zephyr,sram. It writes NVM for this image through sd_flash_write().
 * - BLE events are polled in thread mode; the stack's event interrupt only wakes
 *   the main loop for them.
 * - LF clock: the LFXO, 50 ppm. HFXO ramp-up 1500 us.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <psa/crypto.h>

#include <ble.h>
#include <ble_gap.h>
#include <ble_gatts.h>
#include <ble_hci.h>
#include <nrf_error.h>
#include <nrf_sdm.h>
#include <nrf_soc.h>

#include <nrfx.h>

#include "platform.h"
#include "smp.h"
#include "update.h"

LOG_MODULE_REGISTER(app, CONFIG_SAMPLE_BLE_SMP_UPDATER_LOG_LEVEL);

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#define ATT_MTU	       247U
/* The largest SMP packet: one write-without-response or notification payload. */
#define SMP_PACKET_MAX (ATT_MTU - 3U)

#define ADV_INTERVAL_625US	 160U
#define CONN_EVENT_LENGTH_1250US 6U
#define ATTR_TAB_SIZE		 248U
/* The tag of the one connection configuration. BLE_CONN_CFG_TAG_DEFAULT names the
 * stack's defaults and cannot be configured. */
#define CONN_CFG_TAG		 1U
#define VS_UUID_COUNT		 2U
#define HFCLK_LATENCY_US	 1500U
#define HFINT_CALIBRATION_S	 4U

/* 16-bit aliases, octets 12 and 13 of each 128-bit UUID. */
#define SMP_SERVICE_UUID16 0xDC1DU
#define SMP_CHAR_UUID16	   0x7828U

/* 8D53DC1D-1DB7-4CD3-868B-8A527460AA84, little-endian. */
#define SMP_SERVICE_UUID_BYTES                                                                     \
	0x84U, 0xAAU, 0x60U, 0x74U, 0x52U, 0x8AU, 0x8BU, 0x86U, 0xD3U, 0x4CU, 0xB7U, 0x1DU, 0x1DU, \
		0xDCU, 0x53U, 0x8DU

/* DA2E7828-FBCE-4E01-AE9E-261174997C48, little-endian. */
#define SMP_CHAR_UUID_BYTES                                                                        \
	0x48U, 0x7CU, 0x99U, 0x74U, 0x11U, 0x26U, 0x9EU, 0xAEU, 0x01U, 0x4EU, 0xCEU, 0xFBU, 0x28U, \
		0x78U, 0x2EU, 0xDAU

#define DEVICE_NAME_LEN 12U
#define DEVICE_NAME	'I', 'r', 'o', 'n', 'S', 'i', 'd', 'e', ' ', 'D', 'F', 'U'

/* The APP_RAM_BASE this image gives the stack: its RAM ends below it. */
#define APP_RAM_BASE ((uint32_t)DT_REG_ADDR(DT_CHOSEN(zephyr_sram)))

#define SMP_HDR_LEN 8U

_Static_assert(SMP_PACKET_MAX <= BLE_GATTS_VAR_ATTR_LEN_MAX, "the SMP value outgrows an attribute");
_Static_assert(SMP_REQ_MAX <= (SMP_HDR_LEN + 0xFFFFU), "SMP_REQ_MAX outgrows the SMP length");

struct link {
	uint16_t conn_handle;
	uint8_t adv_handle;
	bool is_reset_requested;
	ble_gatts_char_handles_t smp;
};

static struct link link;

static union {
	ble_evt_t evt;
	uint8_t raw[BLE_EVT_LEN_MAX(ATT_MTU)];
} evt_buf;

static uint8_t smp_value[SMP_PACKET_MAX];
static uint8_t smp_rsp[SMP_PACKET_MAX];

/* The request the ATT writes so far have brought. */
static uint8_t smp_req[SMP_REQ_MAX];
static uint32_t smp_req_len;

/* The stack reads advertising data from RAM while it advertises. */
static uint8_t adv_data[] = {
	2U,
	BLE_GAP_AD_TYPE_FLAGS,
	BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
	17U,
	BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE,
	SMP_SERVICE_UUID_BYTES,
};
static uint8_t scan_rsp_data[] = {
	DEVICE_NAME_LEN + 1U,
	BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
	DEVICE_NAME,
};

static const uint8_t device_name[DEVICE_NAME_LEN] = {DEVICE_NAME};

static const ble_uuid128_t smp_service_base = {{SMP_SERVICE_UUID_BYTES}};
static const ble_uuid128_t smp_char_base = {{SMP_CHAR_UUID_BYTES}};

static const ble_gap_adv_data_t adv = {
	.adv_data = {.p_data = adv_data, .len = sizeof(adv_data)},
	.scan_rsp_data = {.p_data = scan_rsp_data, .len = sizeof(scan_rsp_data)},
};

static const ble_gap_adv_params_t adv_params = {
	.properties = {.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED},
	.p_peer_addr = NULL,
	.interval = ADV_INTERVAL_625US,
	.duration = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED,
	.filter_policy = BLE_GAP_ADV_FP_ANY,
	.primary_phy = BLE_GAP_PHY_1MBPS,
};

static const nrf_clock_lf_cfg_t lf_clock = {
	.source = NRF_CLOCK_LF_SRC_XTAL,
	.accuracy = NRF_CLOCK_LF_ACCURACY_50_PPM,
	.hfclk_latency = HFCLK_LATENCY_US,
	.hfint_ctiv = HFINT_CALIBRATION_S,
};

struct stack_cfg {
	uint32_t id;
	ble_cfg_t cfg;
};

/* Every configuration that differs from the stack's default. */
static const struct stack_cfg stack_cfgs[] = {
	{BLE_CONN_CFG_GAP,
	 {.conn_cfg = {.conn_cfg_tag = CONN_CFG_TAG,
		       .params.gap_conn_cfg = {.conn_count = 1U,
					       .event_length = CONN_EVENT_LENGTH_1250US}}}},
	{BLE_CONN_CFG_GATT,
	 {.conn_cfg = {.conn_cfg_tag = CONN_CFG_TAG,
		       .params.gatt_conn_cfg = {.att_mtu = ATT_MTU}}}},
	{BLE_COMMON_CFG_VS_UUID, {.common_cfg = {.vs_uuid_cfg = {.vs_uuid_count = VS_UUID_COUNT}}}},
	{BLE_GAP_CFG_PPCP_INCL_CONFIG,
	 {.gap_cfg = {.ppcp_include_cfg =
			      {.include_cfg = BLE_GAP_CHAR_INCL_CONFIG_EXCLUDE_WITHOUT_SPACE}}}},
	{BLE_GAP_CFG_CAR_INCL_CONFIG,
	 {.gap_cfg = {.car_include_cfg =
			      {.include_cfg = BLE_GAP_CHAR_INCL_CONFIG_EXCLUDE_WITHOUT_SPACE}}}},
	{BLE_GATTS_CFG_SERVICE_CHANGED,
	 {.gatts_cfg = {.service_changed = {.service_changed = 0U}}}},
	{BLE_GATTS_CFG_ATTR_TAB_SIZE,
	 {.gatts_cfg = {.attr_tab_size = {.attr_tab_size = ATTR_TAB_SIZE}}}},
};

static void stack_fault(uint32_t id, uint32_t pc, uint32_t info)
{
	(void)id;
	(void)pc;
	(void)info;
	platform_reset();
}

static bool smp_service_add(void)
{
	ble_uuid_t service = {.uuid = SMP_SERVICE_UUID16};
	ble_uuid_t chr = {.uuid = SMP_CHAR_UUID16};
	uint16_t service_handle = 0U;
	ble_gatts_char_md_t char_md = {.char_props = {.write_wo_resp = 1U, .notify = 1U}};
	ble_gatts_attr_md_t cccd_md = {.vloc = BLE_GATTS_VLOC_STACK};
	ble_gatts_attr_md_t value_md = {.vloc = BLE_GATTS_VLOC_USER, .vlen = 1U};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&value_md.write_perm);
	char_md.p_cccd_md = &cccd_md;

	const ble_gatts_attr_t value = {
		.p_uuid = &chr,
		.p_attr_md = &value_md,
		.max_len = SMP_PACKET_MAX,
		.p_value = smp_value,
	};

	return (sd_ble_uuid_vs_add(&smp_service_base, &service.type) == NRF_SUCCESS) &&
	       (sd_ble_uuid_vs_add(&smp_char_base, &chr.type) == NRF_SUCCESS) &&
	       (sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &service, &service_handle) ==
		NRF_SUCCESS) &&
	       (sd_ble_gatts_characteristic_add(service_handle, &char_md, &value, &link.smp) ==
		NRF_SUCCESS);
}

/* Logs a stack call that failed. */
static bool is_stack_call_ok(uint32_t nrf_err, const char *call)
{
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("%s failed, nrf_error %#x", call, nrf_err);
	}
	return nrf_err == NRF_SUCCESS;
}

static bool stack_enable(void)
{
	uint32_t required_ram_base = APP_RAM_BASE;
	ble_gap_conn_sec_mode_t name_perm;
	ble_version_t version;

	if (!is_stack_call_ok(sd_softdevice_enable(&lf_clock, stack_fault),
			      "sd_softdevice_enable")) {
		return false;
	}
	if (!platform_stack_seed()) {
		LOG_ERR("Seeding the stack failed");
		return false;
	}
	for (size_t i = 0U; i < ARRAY_LEN(stack_cfgs); i++) {
		if (!is_stack_call_ok(
			    sd_ble_cfg_set(stack_cfgs[i].id, &stack_cfgs[i].cfg, APP_RAM_BASE),
			    "sd_ble_cfg_set")) {
			LOG_ERR("Configuration %#x refused", stack_cfgs[i].id);
			return false;
		}
	}
	if (!is_stack_call_ok(sd_ble_enable(&required_ram_base), "sd_ble_enable")) {
		return false;
	}
	if (required_ram_base > APP_RAM_BASE) {
		LOG_ERR("The stack needs RAM up to %#x, the app starts at %#x", required_ram_base,
			APP_RAM_BASE);
		return false;
	}
	if (sd_ble_version_get(&version) == NRF_SUCCESS) {
		LOG_INF("SoftDevice firmware ID %#x, stack RAM up to %#x",
			version.subversion_number, required_ram_base);
	}

	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&name_perm);
	link.adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;

	if (!smp_service_add()) {
		LOG_ERR("Adding the SMP service failed");
		return false;
	}
	return is_stack_call_ok(
		       sd_ble_gap_device_name_set(&name_perm, device_name, sizeof(device_name)),
		       "sd_ble_gap_device_name_set") &&
	       is_stack_call_ok(sd_ble_gap_adv_set_configure(&link.adv_handle, &adv, &adv_params),
				"sd_ble_gap_adv_set_configure");
}

static void advertising_start(void)
{
	(void)sd_ble_gap_adv_start(link.adv_handle, CONN_CFG_TAG);
}

/* Hands a complete SMP request to the SMP server and notifies its response. A reset
 * waits for the notification to go out. */
static void smp_request_serve(const uint8_t *req, uint32_t len)
{
	bool is_reset = false;
	uint16_t rsp_len =
		(uint16_t)smp_packet_serve(req, len, smp_rsp, sizeof(smp_rsp), &is_reset);

	if (rsp_len == 0U) {
		return;
	}

	const ble_gatts_hvx_params_t hvx = {
		.handle = link.smp.value_handle,
		.type = BLE_GATT_HVX_NOTIFICATION,
		.offset = 0U,
		.p_len = &rsp_len,
		.p_data = smp_rsp,
	};

	link.is_reset_requested = is_reset;
	if (is_reset) {
		LOG_INF("Reset requested");
	}
	if ((sd_ble_gatts_hvx(link.conn_handle, &hvx) != NRF_SUCCESS) && is_reset) {
		platform_reset();
	}
}

/* Adds one ATT write to the request it continues, and serves the request once the
 * length in its SMP header is in. */
static void smp_write_join(const uint8_t *data, uint16_t len)
{
	if (len > (SMP_REQ_MAX - smp_req_len)) {
		smp_req_len = 0U;
		return;
	}
	memcpy(&smp_req[smp_req_len], data, len);
	smp_req_len += len;
	if (smp_req_len < SMP_HDR_LEN) {
		return;
	}

	const uint32_t req_len = SMP_HDR_LEN + (((uint32_t)smp_req[2] << 8) | smp_req[3]);

	if ((req_len > SMP_REQ_MAX) || (smp_req_len > req_len)) {
		smp_req_len = 0U;
		return;
	}
	if (smp_req_len == req_len) {
		smp_req_len = 0U;
		smp_request_serve(smp_req, req_len);
	}
}

/* Answers the requests the stack expects a reply to, and serves SMP writes. */
static void ble_event_serve(const ble_evt_t *evt)
{
	/* Every GAP and GATTS event starts with its connection handle. */
	const uint16_t conn = evt->evt.gap_evt.conn_handle;
	static const ble_gap_phys_t phys_1m = {.tx_phys = BLE_GAP_PHY_1MBPS,
					       .rx_phys = BLE_GAP_PHY_1MBPS};

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Connected");
		link.conn_handle = conn;
		smp_req_len = 0U;
		(void)sd_ble_gatts_sys_attr_set(conn, NULL, 0U, 0U);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Disconnected, reason %#x", evt->evt.gap_evt.params.disconnected.reason);
		link.conn_handle = BLE_CONN_HANDLE_INVALID;
		smp_req_len = 0U;
		update_abort();
		if (link.is_reset_requested) {
			platform_reset();
		}
		advertising_start();
		break;
	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		(void)sd_ble_gap_sec_params_reply(conn, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL,
						  NULL);
		break;
	case BLE_GAP_EVT_SEC_INFO_REQUEST:
		(void)sd_ble_gap_sec_info_reply(conn, NULL);
		break;
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
		(void)sd_ble_gap_phy_update(conn, &phys_1m);
		break;
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
		(void)sd_ble_gap_data_length_update(conn, NULL, NULL);
		break;
	case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
		(void)sd_ble_gatts_exchange_mtu_reply(conn, ATT_MTU);
		break;
	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		(void)sd_ble_gatts_sys_attr_set(conn, NULL, 0U, 0U);
		break;
	case BLE_GATTS_EVT_WRITE:
		if (evt->evt.gatts_evt.params.write.handle == link.smp.value_handle) {
			smp_write_join(evt->evt.gatts_evt.params.write.data,
				       evt->evt.gatts_evt.params.write.len);
		}
		break;
	case BLE_GATTS_EVT_HVN_TX_COMPLETE:
		if (link.is_reset_requested) {
			platform_reset();
		}
		break;
	case BLE_GATTS_EVT_TIMEOUT:
		(void)sd_ble_gap_disconnect(conn, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		break;
	default:
		/* An event that needs no reply. */
		break;
	}
}

/* Serves the BLE events the stack holds. Its SoC events go to the platform from the
 * event interrupt, which also wakes the main loop. */
static void stack_events_drain(void)
{
	for (;;) {
		uint16_t len = (uint16_t)sizeof(evt_buf);

		if (sd_ble_evt_get(evt_buf.raw, &len) != NRF_SUCCESS) {
			return;
		}
		ble_event_serve(&evt_buf.evt);
	}
}

int main(void)
{
	link.conn_handle = BLE_CONN_HANDLE_INVALID;

	const psa_status_t psa_status = psa_crypto_init();

	if (psa_status != PSA_SUCCESS) {
		LOG_ERR("psa_crypto_init failed, psa_status %d", psa_status);
	}
	if ((psa_status != PSA_SUCCESS) || !stack_enable()) {
		LOG_ERR("BLE stack bring-up failed");
		log_flush();
		platform_halt();
	}

	/* The stack has set the event interrupt's priority. */
	NVIC_EnableIRQ(SD_EVT_IRQn);
	advertising_start();
	LOG_INF("Advertising, update slot %#x, %u bytes", PLATFORM_SLOT_ADDR, PLATFORM_SLOT_SIZE);

	/* Logging is deferred: the main loop writes it out. */
	for (;;) {
		stack_events_drain();
		log_flush();
		__WFE();
	}
}
