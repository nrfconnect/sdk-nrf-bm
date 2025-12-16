/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_racp Record Access Control Point
 * @{
 * @ingroup ble_sdk_lib
 * @brief Record Access Control Point library.
 */

#ifndef BLE_RACP_H__
#define BLE_RACP_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Record Access Control Point opcodes. */
enum racp_opcode {
	/** Reserved for future use. */
	RACP_OPCODE_RESERVED = 0,
	/** Report stored records. */
	RACP_OPCODE_REPORT_RECS = 1,
	/** Delete stored records. */
	RACP_OPCODE_DELETE_RECS = 2,
	/** Abort operation. */
	RACP_OPCODE_ABORT_OPERATION = 3,
	/** Report number of stored records. */
	RACP_OPCODE_REPORT_NUM_RECS = 4,
	/** Number of stored records response. */
	RACP_OPCODE_NUM_RECS_RESPONSE = 5,
	/** Response code. */
	RACP_OPCODE_RESPONSE_CODE = 6,
};

/** @brief Record Access Control Point operators. */
enum racp_operator {
	/** Null. */
	RACP_OPERATOR_NULL = 0,
	/** All records. */
	RACP_OPERATOR_ALL = 1,
	/** Less than or equal to. */
	RACP_OPERATOR_LESS_OR_EQUAL = 2,
	/** Greater than or equal to. */
	RACP_OPERATOR_GREATER_OR_EQUAL = 3,
	/** Within range of (inclusive). */
	RACP_OPERATOR_RANGE = 4,
	/** First record (i.e. oldest record). */
	RACP_OPERATOR_FIRST = 5,
	/** Last record (i.e. most recent record). */
	RACP_OPERATOR_LAST = 6,
	/** Start of Reserved for Future Use area. */
	RACP_OPERATOR_RFU_START = 7,
};

/** @brief Record Access Control Point Operand Filter Type Value. */
enum racp_operand_filter_type {
	/** Time Offset. */
	RACP_OPERAND_FILTER_TYPE_TIME_OFFSET = 1,
	/** User Facing Time. */
	RACP_OPERAND_FILTER_TYPE_FACING_TIME = 2,
};

/** @brief Record Access Control Point response codes. */
enum racp_response {
	/** Reserved for future use. */
	RACP_RESPONSE_RESERVED = 0,
	/** Successful operation. */
	RACP_RESPONSE_SUCCESS = 1,
	/** Unsupported op code received. */
	RACP_RESPONSE_OPCODE_UNSUPPORTED = 2,
	/** Operator not valid for service. */
	RACP_RESPONSE_INVALID_OPERATOR = 3,
	/** Unsupported operator. */
	RACP_RESPONSE_OPERATOR_UNSUPPORTED = 4,
	/** Operand not valid for service. */
	RACP_RESPONSE_INVALID_OPERAND = 5,
	/** No matching records found. */
	RACP_RESPONSE_NO_RECORDS_FOUND = 6,
	/** Abort could not be completed. */
	RACP_RESPONSE_ABORT_FAILED = 7,
	/** Procedure could not be completed. */
	RACP_RESPONSE_PROCEDURE_NOT_DONE = 8,
	/** Unsupported operand. */
	RACP_RESPONSE_OPERAND_UNSUPPORTED = 9,
};

/** @brief Record Access Control Point value structure. */
struct ble_racp_value {
	/** Op Code. */
	enum racp_opcode opcode;
	/** Operator. */
	enum racp_operator operator;
	/** Length of the operand. */
	uint8_t operand_len;
	/** Pointer to the operand. */
	uint8_t *operand;
};

/**
 * @brief Decode a Record Access Control Point write.
 *
 * @details This call decodes a write to the Record Access Control Point.
 *
 * @param[in] data Data to decode.
 * @param[in] data_len Length of data.
 * @param[out] racp_val Decoded Record Access Control Point value.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL if @p data or @p racp_val is NULL.
 */
uint32_t ble_racp_decode(const uint8_t *data, size_t data_len, struct ble_racp_value *racp_val);

/**
 * @brief Encode a Record Access Control Point response.
 *
 * @details This call encodes a response from the Record Access Control Point response.
 *
 * @param[in] racp_val Record Access Control Point value to encode.
 * @param[out] buf Buffer for encoded data.
 * @param[out] len Buffer size.
 *
 * @return Length of encoded data or 0 on error.
 */
size_t ble_racp_encode(const struct ble_racp_value *racp_val, uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BLE_RACP_H__ */

/** @} */
