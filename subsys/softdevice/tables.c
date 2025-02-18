/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ble_gap.h>
#include <ble_gatts.h>
#include <ble_gattc.h>
#include <nrf_error.h>
#include <errno.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sd_tables, CONFIG_LOG_DEFAULT_LEVEL);

const char *sd_evt_tostr(int evt)
{
	switch (evt) {
	case BLE_GAP_EVT_CONNECTED:
		return "BLE_GAP_EVT_CONNECTED";
	case BLE_GAP_EVT_DISCONNECTED:
		return "BLE_GAP_EVT_DISCONNECTED";
	case BLE_GAP_EVT_CONN_PARAM_UPDATE:
		return "BLE_GAP_EVT_CONN_PARAM_UPDATE";
	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		return "BLE_GAP_EVT_SEC_PARAMS_REQUEST";
	case BLE_GAP_EVT_SEC_INFO_REQUEST:
		return "BLE_GAP_EVT_SEC_INFO_REQUEST";
	case BLE_GAP_EVT_PASSKEY_DISPLAY:
		return "BLE_GAP_EVT_PASSKEY_DISPLAY";
	case BLE_GAP_EVT_KEY_PRESSED:
		return "BLE_GAP_EVT_KEY_PRESSED";
	case BLE_GAP_EVT_AUTH_KEY_REQUEST:
		return "BLE_GAP_EVT_AUTH_KEY_REQUEST";
	case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
		return "BLE_GAP_EVT_LESC_DHKEY_REQUEST";
	case BLE_GAP_EVT_AUTH_STATUS:
		return "BLE_GAP_EVT_AUTH_STATUS";
	case BLE_GAP_EVT_CONN_SEC_UPDATE:
		return "BLE_GAP_EVT_CONN_SEC_UPDATE";
	case BLE_GAP_EVT_TIMEOUT:
		return "BLE_GAP_EVT_TIMEOUT";
	case BLE_GAP_EVT_RSSI_CHANGED:
		return "BLE_GAP_EVT_RSSI_CHANGED";
	case BLE_GAP_EVT_ADV_REPORT:
		return "BLE_GAP_EVT_ADV_REPORT";
	case BLE_GAP_EVT_SEC_REQUEST:
		return "BLE_GAP_EVT_SEC_REQUEST";
	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
		return "BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST";
	case BLE_GAP_EVT_SCAN_REQ_REPORT:
		return "BLE_GAP_EVT_SCAN_REQ_REPORT";
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
		return "BLE_GAP_EVT_PHY_UPDATE_REQUEST";
	case BLE_GAP_EVT_PHY_UPDATE:
		return "BLE_GAP_EVT_PHY_UPDATE";
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
		return "BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST";
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
		return "BLE_GAP_EVT_DATA_LENGTH_UPDATE";
	case BLE_GAP_EVT_QOS_CHANNEL_SURVEY_REPORT:
		return "BLE_GAP_EVT_QOS_CHANNEL_SURVEY_REPORT";
	case BLE_GAP_EVT_ADV_SET_TERMINATED:
		return "BLE_GAP_EVT_ADV_SET_TERMINATED";
	default:
		return "unknown";
	}
}

const char *sd_error_tostr(int err)
{
	switch (err) {
	case NRF_SUCCESS:
		return "NRF_SUCCESS";
	case NRF_ERROR_SVC_HANDLER_MISSING:
		return "NRF_ERROR_SVC_HANDLER_MISSING";
	case NRF_ERROR_SOFTDEVICE_NOT_ENABLED:
		return "NRF_ERROR_SOFTDEVICE_NOT_ENABLED";
	case NRF_ERROR_INTERNAL:
		return "NRF_ERROR_INTERNAL";
	case NRF_ERROR_NO_MEM:
		return "NRF_ERROR_NO_MEM";
	case NRF_ERROR_NOT_FOUND:
		return "NRF_ERROR_NOT_FOUND";
	case NRF_ERROR_NOT_SUPPORTED:
		return "NRF_ERROR_NOT_SUPPORTED";
	case NRF_ERROR_INVALID_PARAM:
		return "NRF_ERROR_INVALID_PARAM";
	case NRF_ERROR_INVALID_STATE:
		return "NRF_ERROR_INVALID_STATE";
	case NRF_ERROR_INVALID_LENGTH:
		return "NRF_ERROR_INVALID_LENGTH";
	case NRF_ERROR_INVALID_FLAGS:
		return "NRF_ERROR_INVALID_FLAGS";
	case NRF_ERROR_INVALID_DATA:
		return "NRF_ERROR_INVALID_DATA";
	case NRF_ERROR_DATA_SIZE:
		return "NRF_ERROR_DATA_SIZE";
	case NRF_ERROR_TIMEOUT:
		return "NRF_ERROR_TIMEOUT";
	case NRF_ERROR_NULL:
		return "NRF_ERROR_NULL";
	case NRF_ERROR_FORBIDDEN:
		return "NRF_ERROR_FORBIDDEN";
	case NRF_ERROR_INVALID_ADDR:
		return "NRF_ERROR_INVALID_ADDR";
	case NRF_ERROR_BUSY:
		return "NRF_ERROR_BUSY";
	case NRF_ERROR_CONN_COUNT:
		return "NRF_ERROR_CONN_COUNT";
	case NRF_ERROR_RESOURCES:
		return "NRF_ERROR_RESOURCES";

	/* BLE errors */
	case BLE_ERROR_NOT_ENABLED:
		return "BLE_ERROR_NOT_ENABLED";
	case BLE_ERROR_INVALID_CONN_HANDLE:
		return "BLE_ERROR_INVALID_CONN_HANDLE";
	case BLE_ERROR_INVALID_ATTR_HANDLE:
		return "BLE_ERROR_INVALID_ATTR_HANDLE";
	case BLE_ERROR_INVALID_ADV_HANDLE:
		return "BLE_ERROR_INVALID_ADV_HANDLE";
	case BLE_ERROR_INVALID_ROLE:
		return "BLE_ERROR_INVALID_ROLE";
	case BLE_ERROR_BLOCKED_BY_OTHER_LINKS:
		return "BLE_ERROR_BLOCKED_BY_OTHER_LINKS";

	default:
		return "unknown";
	}
}

const int sd_error_to_errno(int sd_error)
{
	LOG_DBG("SoftDevice error %d (%s)", sd_error, sd_error_tostr(sd_error));
	switch (sd_error) {
	case NRF_SUCCESS:
		return 0;
	case NRF_ERROR_SVC_HANDLER_MISSING:
		return -EXDEV;
	case NRF_ERROR_SOFTDEVICE_NOT_ENABLED:
		return -ENOSYS;
	case NRF_ERROR_INTERNAL:
		return -EIO;
	case NRF_ERROR_NO_MEM:
		return -ENOMEM;
	case NRF_ERROR_NOT_FOUND:
		return -EBADF;
	case NRF_ERROR_NOT_SUPPORTED:
		return -ENOTSUP;
	case NRF_ERROR_INVALID_PARAM:
		return -EINVAL;
	case NRF_ERROR_INVALID_STATE:
		return -EPIPE;
	case NRF_ERROR_INVALID_LENGTH:
		return -ERANGE;
	case NRF_ERROR_INVALID_FLAGS:
		return -EPROTOTYPE;
	case NRF_ERROR_INVALID_DATA:
		return -EBADMSG;
	case NRF_ERROR_DATA_SIZE:
		return -EMSGSIZE;
	case NRF_ERROR_TIMEOUT:
		return -ETIMEDOUT;
	case NRF_ERROR_NULL:
		return -EFAULT;
	case NRF_ERROR_FORBIDDEN:
		return -EPERM;
	case NRF_ERROR_INVALID_ADDR:
		return -EADDRNOTAVAIL;
	case NRF_ERROR_BUSY:
		return -EBUSY;
	case NRF_ERROR_CONN_COUNT:
		return -EMLINK;
	case NRF_ERROR_RESOURCES:
		return -EAGAIN;

	/* BLE errors */
	case BLE_ERROR_NOT_ENABLED:
		return -ESRCH;
	case BLE_ERROR_INVALID_CONN_HANDLE:
		return -ENOTCONN;
	case BLE_ERROR_INVALID_ATTR_HANDLE:
		return -ENOENT;
	case BLE_ERROR_INVALID_ADV_HANDLE:
		return -EINVAL; /* TODO duplicate */
	case BLE_ERROR_INVALID_ROLE:
		return -ENODEV;
	case BLE_ERROR_BLOCKED_BY_OTHER_LINKS:
		return -EWOULDBLOCK; /* TODO duplicates EAGAIN */
	default:
		break;
	}

	__ASSERT(false, "Unknown sd error %d", sd_error);
	LOG_ERR("SoftDevice returned error %d, translated to -EIO", sd_error);
	return -EIO; /* Bad error */
}
