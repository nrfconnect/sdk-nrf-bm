/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <ble_racp.h>
#include <stdlib.h>

void ble_racp_decode(uint8_t data_len, uint8_t const *data, struct ble_racp_value *racp_val)
{
	racp_val->opcode = 0xFF;
	racp_val->operator = 0xFF;
	racp_val->operand_len = 0;
	racp_val->operand = NULL;

	if (data_len > 0) {
		racp_val->opcode = data[0];
	}

	if (data_len > 1) {
		racp_val->operator = data[1];
	}

	if (data_len > 2) {
		racp_val->operand_len = data_len - 2;
		racp_val->operand = (uint8_t *)&data[2];
	}
}

uint8_t ble_racp_encode(const struct ble_racp_value *racp_val, uint8_t *data)
{
	uint8_t len = 0;
	int i;

	if (data != NULL) {
		data[len++] = racp_val->opcode;
		data[len++] = racp_val->operator;

		for (i = 0; i < racp_val->operand_len; i++) {
			data[len++] = racp_val->operand[i];
		}
	}

	return len;
}
