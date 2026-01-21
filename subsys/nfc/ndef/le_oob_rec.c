/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <bm/nfc/ndef/le_oob_rec.h>

#define AD_TYPE_FIELD_SIZE 1UL
#define AD_LEN_FIELD_SIZE 1UL

#define LE_ROLE_PAYLOAD_SIZE 1UL

struct bt_data {
	uint8_t type;
	uint8_t data_len;
	const uint8_t *data;
};

static int bt_data_encode(struct bt_data *ad, uint8_t **buff, size_t *size)
{
	const size_t ad_len = ad->data_len + AD_LEN_FIELD_SIZE
		+ AD_TYPE_FIELD_SIZE;

	if (*size < ad_len) {
		return -ENOMEM;
	}

	**buff = ad->data_len + AD_TYPE_FIELD_SIZE;
	*buff += AD_LEN_FIELD_SIZE;

	**buff = ad->type;
	*buff += AD_TYPE_FIELD_SIZE;

	memcpy(*buff, ad->data, ad->data_len);
	*buff += ad->data_len;

	*size -= ad_len;

	return 0;
}

static int ble_device_addr_encode(const ble_gap_addr_t *dev_addr, uint8_t **buff,
				  size_t *size)
{
	int err;
	struct bt_data dev_addr_ad;
	uint8_t dev_addr_buff[sizeof(*dev_addr)];

	if (!dev_addr) {
		return -EINVAL;
	}

	memcpy(dev_addr_buff, &dev_addr->addr[0], sizeof(dev_addr->addr));
	dev_addr_buff[sizeof(dev_addr->addr)] = dev_addr->addr_type;

	dev_addr_ad.type = BLE_GAP_AD_TYPE_LE_BLUETOOTH_DEVICE_ADDRESS;
	dev_addr_ad.data_len = sizeof(dev_addr_buff);
	dev_addr_ad.data = (const uint8_t *) dev_addr_buff;

	err = bt_data_encode(&dev_addr_ad, buff, size);
	if (err) {
		return err;
	}

	return 0;
}

static int le_role_encode(enum nfc_ndef_le_oob_rec_le_role le_role, uint8_t **buff,
			  size_t *size)
{
	int err;
	struct bt_data le_role_ad;

	if (le_role >= NFC_NDEF_LE_OOB_REC_LE_ROLE_OPTIONS_NUM) {
		return -EINVAL;
	}

	le_role_ad.type = BLE_GAP_AD_TYPE_LE_ROLE;
	le_role_ad.data_len = LE_ROLE_PAYLOAD_SIZE;
	le_role_ad.data = (const uint8_t *) &le_role;

	err = bt_data_encode(&le_role_ad, buff, size);
	if (err) {
		return err;
	}

	return 0;
}

int nfc_ndef_le_oob_rec_payload_constructor(
	const struct nfc_ndef_le_oob_rec_payload_desc *payload_desc, uint8_t *buff,
	uint32_t *len)
{
	int err;
	size_t rem_size = *len;

	if (!payload_desc || !payload_desc->addr || !payload_desc->le_role) {
		return -EINVAL;
	}

	err = ble_device_addr_encode(payload_desc->addr, &buff, &rem_size);
	if (err) {
		return err;
	}

	err = le_role_encode(*payload_desc->le_role, &buff, &rem_size);
	if (err) {
		return err;
	}

	if (payload_desc->tk_value) {
		struct bt_data tk;

		tk.type = BLE_GAP_AD_TYPE_SECURITY_MANAGER_TK_VALUE;
		tk.data_len = NFC_NDEF_LE_OOB_REC_TK_LEN;
		tk.data = payload_desc->tk_value;

		err = bt_data_encode(&tk, &buff, &rem_size);
		if (err) {
			return err;
		}
	}

	if (payload_desc->le_sc_data) {
		struct bt_data le_sc_ad;

		le_sc_ad.type = BLE_GAP_AD_TYPE_LESC_CONFIRMATION_VALUE;
		le_sc_ad.data_len =
			sizeof(payload_desc->le_sc_data->c);
		le_sc_ad.data = payload_desc->le_sc_data->c;

		err = bt_data_encode(&le_sc_ad, &buff, &rem_size);
		if (err) {
			return err;
		}

		le_sc_ad.type = BLE_GAP_AD_TYPE_LESC_RANDOM_VALUE;
		le_sc_ad.data_len =
			sizeof(payload_desc->le_sc_data->r);
		le_sc_ad.data = payload_desc->le_sc_data->r;

		err = bt_data_encode(&le_sc_ad, &buff, &rem_size);
		if (err) {
			return err;
		}
	}

	if (payload_desc->appearance) {
		struct bt_data appearance_ad = {
			.type = BLE_GAP_AD_TYPE_APPEARANCE,
			.data_len = sizeof(*payload_desc->appearance),
			.data = (const uint8_t *) payload_desc->appearance,
		};

		err = bt_data_encode(&appearance_ad, &buff, &rem_size);
		if (err) {
			return err;
		}
	}

	if (payload_desc->flags) {
		struct bt_data flags_ad = {
			.type = BLE_GAP_AD_TYPE_FLAGS,
			.data_len = sizeof(*payload_desc->flags),
			.data = payload_desc->flags,
		};

		if ((*payload_desc->flags & BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED) == 0) {
			return -EINVAL;
		}

		err = bt_data_encode(&flags_ad, &buff, &rem_size);
		if (err) {
			return err;
		}
	}

	if (payload_desc->local_name) {
		struct bt_data local_name = {
			.type = BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
			.data_len = strlen(payload_desc->local_name),
			.data = payload_desc->local_name,
		};

		err = bt_data_encode(&local_name, &buff, &rem_size);
		if (err) {
			return err;
		}
	}

	*len = *len - rem_size;

	return 0;
}
