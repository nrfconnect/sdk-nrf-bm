/*
 * Copyright (c) 2012 - 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdint.h>
#include <string.h>
#include <ble_gatts.h>
#include <bm/bluetooth/services/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/__assert.h>
#include <bm/bluetooth/services/ble_dis.h>

#define SYS_ID_LEN 8 /* Length of System ID Characteristic Value */
#define PNP_ID_LEN 7 /* Length of PnP ID Characteristic Value */
#define IEEE_CERT_LEN 8 /* Maximum number of IEEE certifications */

#define gatt_char(_uuid, _value, ...)                                                              \
	{                                                                                          \
		.uuid = _uuid,                                                                     \
		.value = _value,                                                                   \
		COND_CODE_1(IS_EMPTY(__VA_ARGS__), (.len = strlen(_value),),                       \
			    (.len = __VA_ARGS__,))                                                 \
	}

/* Resolve the value of a string characteristic:
 *  - No run-time values (vals == NULL): use the Kconfig build-time default.
 *  - Run-time values given: use the field, or "" when the field is NULL.
 *  - An empty value ("") is skipped later, which omits the characteristic.
 */
#define resolve_str(vals, runtime_val, kconfig_val)                                                \
	(vals == NULL ? kconfig_val :                                                              \
		runtime_val != NULL ? runtime_val :                                                \
			"")

LOG_MODULE_REGISTER(ble_dis, CONFIG_BLE_DIS_LOG_LEVEL);

/* Encode a run-time System ID into its on-air representation. */
static void sys_id_encode(uint8_t *buf, const struct ble_dis_sys_id *id)
{
	sys_put_le40(id->manufacturer_id, &buf[0]);
	sys_put_le24(id->organizationally_unique_id, &buf[5]);
}

/* Encode a run-time PnP ID into its on-air representation. */
static void pnp_id_encode(uint8_t *buf, const struct ble_dis_pnp_id *id)
{
	buf[0] = id->vendor_id_source;
	sys_put_le16(id->vendor_id, &buf[1]);
	sys_put_le16(id->product_id, &buf[3]);
	sys_put_le16(id->product_version, &buf[5]);
}

uint32_t ble_dis_init(const struct ble_dis_config *dis_config)
{
	uint32_t nrf_err;
	uint16_t service_handle;
	ble_uuid_t ble_uuid;
	uint8_t sys_id_buf[SYS_ID_LEN];
	uint8_t pnp_id_buf[PNP_ID_LEN];
	const uint8_t *sys_id_val = NULL;
	const uint8_t *pnp_id_val = NULL;
	const uint8_t *reg_cert_val = NULL;
	uint16_t sys_id_len = 0;
	uint16_t pnp_id_len = 0;
	uint16_t reg_cert_len = 0;

	if (!dis_config) {
		return NRF_ERROR_NULL;
	}

	/* Run-time characteristic values, or NULL to fall back to the Kconfig defaults. */
	const struct ble_dis_values *vals = dis_config->values;

	ble_gatts_char_handles_t char_handles = {0};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = dis_config->sec_mode.device_info_char.read,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &ble_uuid,
		.p_attr_md = &attr_md,
	};

	/* Get the binary values from the runtime config if it was given,
	 * otherwise fall back to the Kconfig defaults.
	 */
	if (vals) {
		if (vals->sys_id != NULL) {
			sys_id_encode(sys_id_buf, vals->sys_id);
			sys_id_val = sys_id_buf;
			sys_id_len = sizeof(sys_id_buf);
		}
		if (vals->pnp_id != NULL) {
			pnp_id_encode(pnp_id_buf, vals->pnp_id);
			pnp_id_val = pnp_id_buf;
			pnp_id_len = sizeof(pnp_id_buf);
		}
		if (vals->reg_cert != NULL) {
			reg_cert_val = vals->reg_cert->list;
			reg_cert_len = vals->reg_cert->len;
		}
	} else {
#if CONFIG_BLE_DIS_SYSTEM_ID
		static const struct ble_dis_sys_id kconfig_sys_id = {
			.manufacturer_id            = CONFIG_BLE_DIS_SYSTEM_ID_MID,
			.organizationally_unique_id = CONFIG_BLE_DIS_SYSTEM_ID_OUI,
		};
		sys_id_encode(sys_id_buf, &kconfig_sys_id);
		sys_id_val = sys_id_buf;
		sys_id_len = sizeof(sys_id_buf);
#endif
#if CONFIG_BLE_DIS_PNP_ID
		static const struct ble_dis_pnp_id kconfig_pnp_id = {
			.vendor_id_source = CONFIG_BLE_DIS_PNP_VID_SRC,
			.vendor_id        = CONFIG_BLE_DIS_PNP_VID,
			.product_id       = CONFIG_BLE_DIS_PNP_PID,
			.product_version  = CONFIG_BLE_DIS_PNP_VER,
		};
		pnp_id_encode(pnp_id_buf, &kconfig_pnp_id);
		pnp_id_val = pnp_id_buf;
		pnp_id_len = sizeof(pnp_id_buf);
#endif
#if CONFIG_BLE_DIS_REGULATORY_CERT
		static const uint8_t kconfig_reg_cert[IEEE_CERT_LEN] =
			sys_uint64_to_array(CONFIG_BLE_DIS_REGULATORY_CERT_LIST);
		reg_cert_val = kconfig_reg_cert;
		reg_cert_len = sizeof(kconfig_reg_cert);
#endif
	}

	/* Put all characteristics in one table.
	 * Empty ones (zero length) are skipped later, so they won't be added.
	 */
	const struct {
		const uint8_t *value;
		uint16_t uuid;
		uint16_t len;
	} chars[] = {
		gatt_char(BLE_UUID_MANUFACTURER_NAME_STRING_CHAR,
			  resolve_str(vals, vals->manufacturer_name,
				      CONFIG_BLE_DIS_MANUFACTURER_NAME)),
		gatt_char(BLE_UUID_MODEL_NUMBER_STRING_CHAR,
			  resolve_str(vals, vals->model_number,
				      CONFIG_BLE_DIS_MODEL_NUMBER)),
		gatt_char(BLE_UUID_SERIAL_NUMBER_STRING_CHAR,
			  resolve_str(vals, vals->serial_number,
				      CONFIG_BLE_DIS_SERIAL_NUMBER)),
		gatt_char(BLE_UUID_HARDWARE_REVISION_STRING_CHAR,
			  resolve_str(vals, vals->hw_revision,
				      CONFIG_BLE_DIS_HW_REVISION)),
		gatt_char(BLE_UUID_FIRMWARE_REVISION_STRING_CHAR,
			  resolve_str(vals, vals->fw_revision,
				      CONFIG_BLE_DIS_FW_REVISION)),
		gatt_char(BLE_UUID_SOFTWARE_REVISION_STRING_CHAR,
			  resolve_str(vals, vals->sw_revision,
				      CONFIG_BLE_DIS_SW_REVISION)),

		gatt_char(BLE_UUID_SYSTEM_ID_CHAR,
			  sys_id_val, sys_id_len),
		gatt_char(BLE_UUID_PNP_ID_CHAR,
			  pnp_id_val, pnp_id_len),
		gatt_char(BLE_UUID_IEEE_REGULATORY_CERTIFICATION_DATA_LIST_CHAR,
			  reg_cert_val, reg_cert_len),
	};

	/* Add service */
	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_DEVICE_INFORMATION_SERVICE);

	nrf_err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &service_handle);
	if (nrf_err) {
		LOG_ERR("Failed to add device information service, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	for (size_t i = 0; i < ARRAY_SIZE(chars); i++) {
		if (!chars[i].len) {
			/* Skip characteristics with no value (omitted by config or Kconfig). */
			continue;
		}

		BLE_UUID_BLE_ASSIGN(ble_uuid, chars[i].uuid);
		attr_char_value.p_value = (uint8_t *)chars[i].value;
		attr_char_value.max_len = chars[i].len;
		attr_char_value.init_len = chars[i].len;

		LOG_DBG("Added char %#x, len %d", chars[i].uuid, chars[i].len);

		nrf_err = sd_ble_gatts_characteristic_add(service_handle, &char_md,
							  &attr_char_value, &char_handles);
		if (nrf_err) {
			LOG_ERR("Failed to add characteristic, nrf_error %#x", nrf_err);
			return nrf_err;
		}
	}

	return NRF_SUCCESS;
}
