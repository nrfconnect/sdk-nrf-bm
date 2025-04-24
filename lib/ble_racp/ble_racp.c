/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <ble_racp.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

int ble_racp_decode(const uint8_t *data, size_t data_len, struct ble_racp_value *racp_val)
{
	if (!data || !racp_val) {
		return -EFAULT;
	}

	racp_val->opcode = (data_len >= 1) ? data[0] : 0xFF;
	racp_val->operator = (data_len >= 2) ? data[1] : 0xFF;
	racp_val->operand_len = (data_len >= 3) ? (data_len - 2) : 0;
	racp_val->operand = (data_len >= 3) ? (uint8_t *)&data[2] : NULL;

	return 0;
}

int ble_racp_encode(const struct ble_racp_value *racp_val, uint8_t *buf, size_t buf_len)
{
	uint8_t len = 0;

	if (!racp_val || !buf) {
		return -EFAULT;
	}

	if (buf_len < (racp_val->operand_len + 2)) {
		return -EINVAL;
	}

	buf[len++] = racp_val->opcode;
	buf[len++] = racp_val->operator;

	for (int i = 0; i < racp_val->operand_len; i++) {
		buf[len++] = racp_val->operand[i];
	}

	return len;
}
