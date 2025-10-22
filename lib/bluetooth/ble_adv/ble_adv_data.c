/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdint.h>
#include <string.h>
#include <ble_gap.h>
#include <bm/bluetooth/ble_adv_data.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Advertising Data and Scan Response format contains 1 octet for the length. */
#define AD_LENGTH_FIELD_SIZE 1UL
/* Advertising Data and Scan Response format contains 1 octet for the AD type. */
#define AD_TYPE_FIELD_SIZE   1UL
/* Offset for the AD data field of the Advertising Data and Scan Response format. */
#define AD_DATA_OFFSET	     (AD_LENGTH_FIELD_SIZE + AD_TYPE_FIELD_SIZE)

/* Data size (in octets) of the Address type of the LE Bluetooth Device Address */
#define AD_TYPE_BLE_DEVICE_ADDR_TYPE_SIZE 1UL
/* Data size (in octets) of the LE Bluetooth Device Address */
#define AD_TYPE_BLE_DEVICE_ADDR_DATA_SIZE (BLE_GAP_ADDR_LEN + AD_TYPE_BLE_DEVICE_ADDR_TYPE_SIZE)
/* Size (in octets) of the LE Bluetooth Device Address */
#define AD_TYPE_BLE_DEVICE_ADDR_SIZE	  (AD_DATA_OFFSET + AD_TYPE_BLE_DEVICE_ADDR_DATA_SIZE)
/* Data size (in octets) of the Appearance */
#define AD_TYPE_APPEARANCE_DATA_SIZE	  2UL
/* Size (in octets) of the Appearance */
#define AD_TYPE_APPEARANCE_SIZE		  (AD_DATA_OFFSET + AD_TYPE_APPEARANCE_DATA_SIZE)
/* Data size (in octets) of the Flags */
#define AD_TYPE_FLAGS_DATA_SIZE		  1UL
/* Size (in octets) of the Flags */
#define AD_TYPE_FLAGS_SIZE		  (AD_DATA_OFFSET + AD_TYPE_FLAGS_DATA_SIZE)
/* Data size (in octets) of the TX Power Level */
#define AD_TYPE_TX_POWER_LEVEL_DATA_SIZE  1UL
/* Size (in octets) of the TX Power Level */
#define AD_TYPE_TX_POWER_LEVEL_SIZE	  (AD_DATA_OFFSET + AD_TYPE_TX_POWER_LEVEL_DATA_SIZE)
/* Data size (in octets) of the Slave Connection Interval Range */
#define AD_TYPE_CONN_INT_DATA_SIZE	  4UL
/* Data size (in octets) of the Slave Connection Interval Range */
#define AD_TYPE_CONN_INT_SIZE		  (AD_DATA_OFFSET + AD_TYPE_CONN_INT_DATA_SIZE)
/* Size (in octets) of the Company Identifier Code, part of the Manufacturer Specific Data */
#define AD_TYPE_MANUF_SPEC_DATA_ID_SIZE	  2UL
/* Size (in octets) of the 16-bit UUID, which is a part of the Service Data */
#define AD_TYPE_SERV_DATA_16BIT_UUID_SIZE 2UL

#define BLE_ADV_DATA_MATCH_FULL_NAME       0xff

#define AD_TYPE_BLE_DEVICE_ADDR_TYPE_PUBLIC 0UL
#define AD_TYPE_BLE_DEVICE_ADDR_TYPE_RANDOM 1UL

#define UUID16_SIZE  2	/* Size of 16 bit UUID, in bytes */
#define UUID32_SIZE  4	/* Size of 32 bit UUID, in bytes */
#define UUID128_SIZE 16 /* Size of 128 bit UUID, in bytes */

#define N_AD_TYPES 2 /* The number of Advertising data types to search for at a time. */

LOG_MODULE_REGISTER(ble_adv_data);

static uint32_t device_addr_encode(uint8_t *buf, uint16_t *offset, uint16_t max_size)
{
	uint32_t nrf_err;
	ble_gap_addr_t device_addr;

	/* Check for buffer overflow */
	if (*offset + AD_TYPE_BLE_DEVICE_ADDR_SIZE > max_size) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* Get BLE address */
	nrf_err = sd_ble_gap_addr_get(&device_addr);
	if (nrf_err) {
		LOG_ERR("Failed to get device GAP address, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/* Encode BLE device address */
	buf[*offset] = (uint8_t)(AD_TYPE_FIELD_SIZE + AD_TYPE_BLE_DEVICE_ADDR_DATA_SIZE);
	*offset += AD_LENGTH_FIELD_SIZE;

	buf[*offset] = BLE_GAP_AD_TYPE_LE_BLUETOOTH_DEVICE_ADDRESS;
	*offset += AD_TYPE_FIELD_SIZE;

	memcpy(&buf[*offset], device_addr.addr, BLE_GAP_ADDR_LEN);
	*offset += BLE_GAP_ADDR_LEN;

	if (BLE_GAP_ADDR_TYPE_PUBLIC == device_addr.addr_type) {
		buf[*offset] = AD_TYPE_BLE_DEVICE_ADDR_TYPE_PUBLIC;
	} else {
		buf[*offset] = AD_TYPE_BLE_DEVICE_ADDR_TYPE_RANDOM;
	}
	*offset += AD_TYPE_BLE_DEVICE_ADDR_TYPE_SIZE;

	return NRF_SUCCESS;
}

static uint32_t device_name_encode(const struct ble_adv_data *ble_adv_data, uint8_t *data,
				   uint16_t *offset, uint16_t max_size)
{
	uint32_t nrf_err;
	uint16_t rem_adv_data_len;
	uint16_t actual_length;
	uint8_t adv_data_format;

	/* Validate parameters */
	if ((ble_adv_data->name_type == BLE_ADV_DATA_SHORT_NAME) &&
	    (ble_adv_data->short_name_len == 0)) {
		return NRF_ERROR_INVALID_PARAM;
	}

	/* Check for buffer overflow */
	if ((*offset + AD_DATA_OFFSET > max_size) ||
	    ((ble_adv_data->name_type == BLE_ADV_DATA_SHORT_NAME) &&
	     ((*offset + AD_DATA_OFFSET + ble_adv_data->short_name_len) > max_size))) {
		return NRF_ERROR_DATA_SIZE;
	}

	rem_adv_data_len = max_size - *offset - AD_DATA_OFFSET;
	actual_length = rem_adv_data_len;

	/* Get GAP device name and length */
	nrf_err = sd_ble_gap_device_name_get(&data[*offset + AD_DATA_OFFSET], &actual_length);
	if (nrf_err) {
		LOG_ERR("Failed to get device GAP name, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/* Check if device intend to use short name and it can fit available data size.
	 * If the name is shorter than the preferred short name length then it is no longer
	 * a short name and is in fact the complete name of the device.
	 */
	if (((ble_adv_data->name_type == BLE_ADV_DATA_FULL_NAME) ||
	     (actual_length <= ble_adv_data->short_name_len)) &&
	    (actual_length <= rem_adv_data_len)) {
		/* Complete device name can fit */
		adv_data_format = BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
	} else {
		/* Use short name if complete name doesn't fit or if explicitly configure */
		adv_data_format = BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME;

		/* If application has set a preference on the short name size,
		 * it needs to be considered, else fit what can be fit.
		 */
		if ((ble_adv_data->name_type == BLE_ADV_DATA_SHORT_NAME) &&
		    (ble_adv_data->short_name_len <= rem_adv_data_len)) {
			/* Short name fits available size */
			actual_length = ble_adv_data->short_name_len;
		} else {
			/* Whatever can fit the data buffer will be packed */
			actual_length = rem_adv_data_len;
		}
	}

	/* There is only 1 byte intended to encode length which is
	 * (actual_length + AD_TYPE_FIELD_SIZE)
	 */
	if (actual_length > (0x00FF - AD_TYPE_FIELD_SIZE)) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* Complete name field in encoded data. */
	data[*offset] = (uint8_t)(AD_TYPE_FIELD_SIZE + actual_length);
	*offset += AD_LENGTH_FIELD_SIZE;

	data[*offset] = adv_data_format;
	*offset += AD_TYPE_FIELD_SIZE;
	*offset += actual_length;

	return NRF_SUCCESS;
}

static uint32_t appearance_encode(uint8_t *buf, uint16_t *offset, uint16_t max_size)
{
	uint32_t nrf_err;
	uint16_t appearance;

	/* Check for buffer overflow */
	if (*offset + AD_TYPE_APPEARANCE_SIZE > max_size) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* Get GAP appearance field */
	nrf_err = sd_ble_gap_appearance_get(&appearance);
	if (nrf_err) {
		LOG_ERR("Failed to get GAP appearance, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/* Encode Length, AD Type and Appearance */
	buf[*offset] = (uint8_t)(AD_TYPE_FIELD_SIZE + AD_TYPE_APPEARANCE_DATA_SIZE);
	*offset += AD_LENGTH_FIELD_SIZE;

	buf[*offset] = BLE_GAP_AD_TYPE_APPEARANCE;
	*offset += AD_TYPE_FIELD_SIZE;

	sys_put_le16(appearance, &buf[*offset]);
	*offset += sizeof(uint16_t);

	return NRF_SUCCESS;
}

static uint32_t flags_encode(int8_t flags, uint8_t *buf, uint16_t *offset, uint16_t max_size)
{
	/* Check for buffer overflow */
	if (*offset + AD_TYPE_FLAGS_SIZE > max_size) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* Encode flags */
	buf[*offset] = (uint8_t)(AD_TYPE_FIELD_SIZE + AD_TYPE_FLAGS_DATA_SIZE);
	*offset += AD_LENGTH_FIELD_SIZE;

	buf[*offset] = BLE_GAP_AD_TYPE_FLAGS;
	*offset += AD_TYPE_FIELD_SIZE;

	buf[*offset] = flags;
	*offset += AD_TYPE_FLAGS_DATA_SIZE;

	return NRF_SUCCESS;
}

static uint32_t tx_power_level_encode(int8_t tx_power_level, uint8_t *buf, uint16_t *offset,
				      uint16_t max_size)
{
	/* Check for buffer overflow */
	if (*offset + AD_TYPE_TX_POWER_LEVEL_SIZE > max_size) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* Encode TX Power Level */
	buf[*offset] = (uint8_t)(AD_TYPE_FIELD_SIZE + AD_TYPE_TX_POWER_LEVEL_DATA_SIZE);
	*offset += AD_LENGTH_FIELD_SIZE;

	buf[*offset] = BLE_GAP_AD_TYPE_TX_POWER_LEVEL;
	*offset += AD_TYPE_FIELD_SIZE;

	buf[*offset] = tx_power_level;
	*offset += AD_TYPE_TX_POWER_LEVEL_DATA_SIZE;

	return NRF_SUCCESS;
}

static uint32_t uuid_list_sized_encode(const struct ble_adv_data_uuid_list *list, uint8_t adv_type,
				       uint8_t uuid_size, uint8_t *buf, uint16_t *offset,
				       uint16_t max_size)
{
	uint32_t nrf_err;
	bool is_heading_written = false;
	uint16_t start_pos = *offset;
	uint16_t length;

	for (uint8_t i = 0; i < list->len; i++) {
		uint8_t encoded_size;
		ble_uuid_t uuid = list->uuid[i];

		/* Find encoded uuid size */
		nrf_err = sd_ble_uuid_encode(&uuid, &encoded_size, NULL);
		if (nrf_err) {
			LOG_ERR("Failed to encode UUID, nrf_error %#x", nrf_err);
			return nrf_err;
		}

		/* Check size */
		if (encoded_size == uuid_size) {
			uint8_t heading_bytes = (is_heading_written) ? 0 : AD_DATA_OFFSET;

			/* Check for buffer overflow */
			if ((*offset + encoded_size + heading_bytes) > max_size) {
				return NRF_ERROR_DATA_SIZE;
			}

			if (!is_heading_written) {
				/* Write AD structure heading */
				*offset += AD_LENGTH_FIELD_SIZE;
				buf[*offset] = adv_type;
				*offset += AD_TYPE_FIELD_SIZE;
				is_heading_written = true;
			}

			/* Write UUID */
			nrf_err = sd_ble_uuid_encode(&uuid, &encoded_size, &buf[*offset]);
			if (nrf_err) {
				LOG_ERR("Failed to encode UUID, nrf_error %#x", nrf_err);
				return nrf_err;
			}

			*offset += encoded_size;
		}
	}

	if (is_heading_written) {
		/* The length field does not count itself. */
		length = *offset - (start_pos + AD_LENGTH_FIELD_SIZE);
		if (length > 0x00FF) {
			return NRF_ERROR_DATA_SIZE;
		}
		buf[start_pos] = (uint8_t)length;
	}

	return NRF_SUCCESS;
}

static uint32_t uuid_list_encode(const struct ble_adv_data_uuid_list *list, uint8_t adv_type_16,
				 uint8_t adv_type_128, uint8_t *buf, uint16_t *offset,
				 uint16_t max_size)
{
	uint32_t nrf_err;

	/* Encode 16 bit UUIDs */
	nrf_err = uuid_list_sized_encode(list, adv_type_16, UUID16_SIZE, buf, offset, max_size);
	if (nrf_err) {
		return nrf_err;
	}

	/* Encode 128 bit UUIDs */
	nrf_err = uuid_list_sized_encode(list, adv_type_128, UUID128_SIZE, buf, offset, max_size);
	if (nrf_err) {
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint32_t conn_int_check(const struct ble_adv_data_conn_int *conn_interval)
{
	/* Check Minimum Connection Interval */
	if ((conn_interval->min_conn_interval < 0x0006) ||
	    ((conn_interval->min_conn_interval > 0x0c80) &&
	     (conn_interval->min_conn_interval != 0xffff))) {
		return NRF_ERROR_INVALID_PARAM;
	}

	/* Check Maximum Connection Interval */
	if ((conn_interval->max_conn_interval < 0x0006) ||
	    ((conn_interval->max_conn_interval > 0x0c80) &&
	     (conn_interval->max_conn_interval != 0xffff))) {
		return NRF_ERROR_INVALID_PARAM;
	}

	/* Check Minimum Connection Interval is smaller than Maximum Connection Interval */
	if ((conn_interval->min_conn_interval != 0xffff) &&
	    (conn_interval->max_conn_interval != 0xffff) &&
	    (conn_interval->min_conn_interval > conn_interval->max_conn_interval)) {
		return NRF_ERROR_INVALID_PARAM;
	}

	return NRF_SUCCESS;
}

static uint32_t conn_int_encode(const struct ble_adv_data_conn_int *conn_int, uint8_t *buf,
				uint16_t *offset, uint16_t max_size)
{
	uint32_t nrf_err;

	/* Check for buffer overflow */
	if (*offset + AD_TYPE_CONN_INT_SIZE > max_size) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* Check parameter */
	nrf_err = conn_int_check(conn_int);
	if (nrf_err) {
		return nrf_err;
	}

	/* Encode Length and AD Type */
	buf[*offset] = (uint8_t)(AD_TYPE_FIELD_SIZE + AD_TYPE_CONN_INT_DATA_SIZE);
	*offset += AD_LENGTH_FIELD_SIZE;

	buf[*offset] = BLE_GAP_AD_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE;
	*offset += AD_TYPE_FIELD_SIZE;

	/* Encode Minimum and Maximum Connection Interval */
	sys_put_le16(conn_int->min_conn_interval, &buf[*offset]);
	*offset += sizeof(uint16_t);
	sys_put_le16(conn_int->max_conn_interval, &buf[*offset]);
	*offset += sizeof(uint16_t);

	return NRF_SUCCESS;
}

static uint32_t manuf_specific_data_encode(const struct ble_adv_data_manufacturer *manuf_data,
					   uint8_t *buf, uint16_t *offset, uint16_t max_size)
{
	uint32_t data_size = AD_TYPE_MANUF_SPEC_DATA_ID_SIZE + manuf_data->len;

	/* Check for buffer overflow */
	if ((*offset + AD_DATA_OFFSET + data_size) > max_size) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* There is only 1 byte intended to encode length which is (data_size + AD_TYPE_FIELD_SIZE) */
	if (data_size > (0x00FF - AD_TYPE_FIELD_SIZE)) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* Encode Length and AD Type */
	buf[*offset] = (uint8_t)(AD_TYPE_FIELD_SIZE + data_size);
	*offset += AD_LENGTH_FIELD_SIZE;
	buf[*offset] = BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA;
	*offset += AD_TYPE_FIELD_SIZE;

	/* Encode Company Identifier */
	sys_put_le16(manuf_data->company_identifier, &buf[*offset]);
	*offset += sizeof(uint16_t);

	/* Encode additional manufacturer specific data */
	if (manuf_data->len > 0) {
		if (!manuf_data->data) {
			return NRF_ERROR_INVALID_PARAM;
		}
		memcpy(&buf[*offset], manuf_data->data, manuf_data->len);
		*offset += manuf_data->len;
	}

	return NRF_SUCCESS;
}

/* Implemented only for 16-bit UUIDs */
static uint32_t service_data_encode(const struct ble_adv_data *ble_adv_data, uint8_t *buf,
				    uint16_t *offset, uint16_t max_size)
{
	uint32_t data_size;
	struct ble_adv_data_service *service_data;

	/* Check parameter consistency */
	if (!ble_adv_data->srv_list.service) {
		return NRF_ERROR_INVALID_PARAM;
	}

	for (uint8_t i = 0; i < ble_adv_data->srv_list.len; i++) {
		service_data = &ble_adv_data->srv_list.service[i];
		/* For now implemented only for 16-bit UUID */
		data_size = AD_TYPE_SERV_DATA_16BIT_UUID_SIZE + service_data->len;

		/* There is only 1 byte intended to encode length which is
		 * (data_size + AD_TYPE_FIELD_SIZE)
		 */
		if (data_size > (0x00FF - AD_TYPE_FIELD_SIZE)) {
			return NRF_ERROR_DATA_SIZE;
		}

		/* Encode Length and AD Type */
		buf[*offset] = (uint8_t)(AD_TYPE_FIELD_SIZE + data_size);
		*offset += AD_LENGTH_FIELD_SIZE;

		buf[*offset] = BLE_GAP_AD_TYPE_SERVICE_DATA;
		*offset += AD_TYPE_FIELD_SIZE;

		/* Encode service 16-bit UUID */
		sys_put_le16(service_data->service_uuid, &buf[*offset]);
		*offset += sizeof(uint16_t);

		/* Encode additional service data */
		if (service_data->len > 0) {
			if (!service_data->data) {
				return NRF_ERROR_INVALID_PARAM;
			}
			memcpy(&buf[*offset], service_data->data, service_data->len);
			*offset += service_data->len;
		}
	}

	return NRF_SUCCESS;
}

uint32_t ble_adv_data_encode(const struct ble_adv_data *ble_adv_data, uint8_t *buf, uint16_t *len)
{
	uint32_t nrf_err;
	uint16_t max_size;

	nrf_err = NRF_SUCCESS;
	max_size = *len;
	*len = 0;

	if (!ble_adv_data || !buf || !len) {
		return NRF_ERROR_NULL;
	}

	/* Encode LE Bluetooth Device Address */
	if (ble_adv_data->include_ble_device_addr) {
		nrf_err = device_addr_encode(buf, len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}
	/* Encode appearance */
	if (ble_adv_data->include_appearance) {
		nrf_err = appearance_encode(buf, len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}
	/* Encode Flag */
	if (ble_adv_data->flags != 0) {
		nrf_err = flags_encode(ble_adv_data->flags, buf, len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}
	/* Encode TX power level */
	if (ble_adv_data->tx_power_level != NULL) {
		nrf_err = tx_power_level_encode(*ble_adv_data->tx_power_level, buf, len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}
	/* Encode 'more available' uuid list */
	if (ble_adv_data->uuid_lists.more_available.len > 0) {
		nrf_err = uuid_list_encode(&ble_adv_data->uuid_lists.more_available,
					   BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE,
					   BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE, buf,
					   len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}
	/* Encode 'complete' uuid list */
	if (ble_adv_data->uuid_lists.complete.len > 0) {
		nrf_err = uuid_list_encode(&ble_adv_data->uuid_lists.complete,
					   BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE,
					   BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE,
					   buf, len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}
	/* Encode 'solicited service' uuid list */
	if (ble_adv_data->uuid_lists.solicited.len > 0) {
		nrf_err = uuid_list_encode(&ble_adv_data->uuid_lists.solicited,
					   BLE_GAP_AD_TYPE_SOLICITED_SERVICE_UUIDS_16BIT,
					   BLE_GAP_AD_TYPE_SOLICITED_SERVICE_UUIDS_128BIT,
					   buf, len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}
	/* Encode Slave Connection Interval Range */
	if (ble_adv_data->slave_conn_int != NULL) {
		nrf_err = conn_int_encode(ble_adv_data->slave_conn_int, buf, len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}
	/* Encode Manufacturer Specific Data */
	if (ble_adv_data->manufacturer_data != NULL) {
		nrf_err = manuf_specific_data_encode(ble_adv_data->manufacturer_data,
						     buf, len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}
	/* Encode Service Data */
	if (ble_adv_data->srv_list.len > 0) {
		nrf_err = service_data_encode(ble_adv_data, buf, len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}
	/* Encode name. It is encoded last on purpose since too long device name is truncated */
	if (ble_adv_data->name_type != BLE_ADV_DATA_NO_NAME) {
		nrf_err = device_name_encode(ble_adv_data, buf, len, max_size);
		if (nrf_err) {
			return nrf_err;
		}
	}

	return nrf_err;
}

uint16_t ble_adv_data_search(const uint8_t *data, uint16_t data_len, uint16_t *offset,
			     uint8_t ad_type)
{
	uint16_t i;
	uint16_t off;
	uint16_t len;

	if (data == NULL || offset == NULL) {
		return 0;
	}

	/* Scroll data until the desired type is found */
	i = 0;
	while ((i + 1 < data_len) && ((i < *offset) || (data[i + 1] != ad_type))) {
		/* Jump to next data */
		i += (data[i] + 1);
	}

	if (i >= data_len) {
		return 0;
	}

	off = i + 2;
	len = data[i] ? (data[i] - 1) : 0;

	if (!len || ((off + len) > data_len)) {
		/* Malformed. Zero len or extends beyond provided data. */
		return 0;
	}

	*offset = off;

	return len;
}

uint8_t *ble_adv_data_parse(const uint8_t *data, uint16_t data_len, uint8_t ad_type)
{
	uint16_t offset;
	uint16_t len;

	offset = 0;
	len = ble_adv_data_search(data, data_len, &offset, ad_type);

	if (len == 0) {
		return NULL;
	}

	return (uint8_t *)&data[offset];
}

bool ble_adv_data_name_find(const uint8_t *data, uint16_t data_len, const char *name)
{
	uint16_t parsed_name_len;
	uint16_t data_offset;

	if (name == NULL || name[0] == '\0') {
		return false;
	}

	data_offset = 0;
	parsed_name_len = ble_adv_data_search(data, data_len, &data_offset,
					      BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME);

	if (data_offset == 0 || parsed_name_len == 0) {
		return false;
	}

	const uint8_t *const parsed_name = &data[data_offset];

	if ((strlen(name) == parsed_name_len) &&
	    (memcmp(name, parsed_name, parsed_name_len) == 0)) {
		return true;
	}

	return false;
}

bool ble_adv_data_short_name_find(const uint8_t *data, uint16_t data_len, const char *name,
				  uint8_t short_name_min_len)
{
	uint16_t parsed_name_len;
	uint16_t data_offset;

	if (!data || !name) {
		return false;
	}

	data_offset = 0;
	parsed_name_len =
		ble_adv_data_search(data, data_len, &data_offset, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME);

	const uint8_t *const parsed_name = &data[data_offset];

	if ((data_offset != 0) && (parsed_name_len != 0) &&
	    (parsed_name_len >= short_name_min_len) && (parsed_name_len < strlen(name)) &&
	    (memcmp(name, parsed_name, parsed_name_len) == 0)) {
		return true;
	}

	return false;
}

bool ble_adv_data_uuid_find(const uint8_t *data, uint16_t data_len, const ble_uuid_t *uuid)
{
	uint32_t nrf_err;
	uint16_t data_offset;
	uint8_t raw_uuid_len;
	uint16_t parsed_uuid_len = data_len;
	uint8_t raw_uuid[UUID128_SIZE];
	uint8_t ad_types[N_AD_TYPES];

	if (!data || !uuid) {
		return false;
	}

	nrf_err = sd_ble_uuid_encode(uuid, &raw_uuid_len, raw_uuid);
	if (nrf_err) {
		/* Invalid encoded data or target UUID */
		return false;
	}

	switch (raw_uuid_len) {
	case UUID16_SIZE:
		ad_types[0] = BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE;
		ad_types[1] = BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE;
		break;

	case UUID32_SIZE:
		/* Not currently supported by sd_ble_uuid_encode() */
		ad_types[0] = BLE_GAP_AD_TYPE_32BIT_SERVICE_UUID_COMPLETE;
		ad_types[1] = BLE_GAP_AD_TYPE_32BIT_SERVICE_UUID_MORE_AVAILABLE;
		break;

	case UUID128_SIZE:
		ad_types[0] = BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE;
		ad_types[1] = BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE;
		break;

	default:
		return false;
	}

	data_offset = 0;
	for (uint8_t i = 0; (i < N_AD_TYPES) && (data_offset == 0); i++) {
		parsed_uuid_len = ble_adv_data_search(data, data_len, &data_offset, ad_types[i]);
	}

	if (data_offset == 0) {
		/* Could not find any relevant UUIDs in the encoded data */
		return false;
	}

	/* Verify if any UUID matches the given UUID. */
	const uint8_t *const parsed_uuid = &data[data_offset];

	for (uint16_t list_offset = 0; list_offset < parsed_uuid_len; list_offset += raw_uuid_len) {
		if (memcmp(&parsed_uuid[list_offset], raw_uuid, raw_uuid_len) == 0) {
			return true;
		}
	}

	/* Could not find the UUID among the encoded data */
	return false;
}

bool ble_adv_data_appearance_find(const uint8_t *data, uint16_t data_len,
				  const uint16_t *target_appearance)
{
	uint16_t data_offset;
	uint8_t appearance_len;
	uint16_t decoded_appearance;

	if (!data || !target_appearance) {
		return false;
	}

	data_offset = 0;
	appearance_len =
		ble_adv_data_search(data, data_len, &data_offset, BLE_GAP_AD_TYPE_APPEARANCE);

	if (data_offset == 0 || appearance_len == 0) {
		return false;
	}

	decoded_appearance = sys_get_le16(&data[data_offset]);
	if (decoded_appearance == *target_appearance) {
		return true;
	}

	return false;
}
