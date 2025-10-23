/*
 * Copyright (c) 2011-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <stdlib.h>
#include <zephyr/sys/util.h>
#include <nrf_error.h>
#include <nrf_strerror.h>

/**
 * @brief Macro for adding an entity to the description array.
 *
 * Macro that helps to create a single entity in the description array.
 */
#define NRF_STRERROR_ENTITY(mnemonic)                                                              \
	{                                                                                          \
		.code = mnemonic, .name = #mnemonic                                                \
	}

/**
 * @brief Array entity element that describes an error.
 */
struct strerror_desc {
	uint32_t code;  /**< Error code. */
	const char *name; /**< Descriptive name (the same as the internal error mnemonic). */
};

/**
 * @brief Unknown error code.
 *
 * The constant string used by @ref nrf_strerror_get when the error description was not found.
 */
static const char unknown_str[] = "Unknown error code";

/**
 * @brief Array with error codes.
 *
 * Array that describes error codes.
 *
 * @note It is required for this array to have error codes placed in ascending order.
 *       This condition is checked in automatic unit test before the release.
 */
static const struct strerror_desc nrf_strerror_array[] = {
	NRF_STRERROR_ENTITY(NRF_SUCCESS), NRF_STRERROR_ENTITY(NRF_ERROR_SVC_HANDLER_MISSING),
	NRF_STRERROR_ENTITY(NRF_ERROR_SOFTDEVICE_NOT_ENABLED),
	NRF_STRERROR_ENTITY(NRF_ERROR_INTERNAL), NRF_STRERROR_ENTITY(NRF_ERROR_NO_MEM),
	NRF_STRERROR_ENTITY(NRF_ERROR_NOT_FOUND), NRF_STRERROR_ENTITY(NRF_ERROR_NOT_SUPPORTED),
	NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_PARAM), NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_STATE),
	NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_LENGTH), NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_FLAGS),
	NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_DATA), NRF_STRERROR_ENTITY(NRF_ERROR_DATA_SIZE),
	NRF_STRERROR_ENTITY(NRF_ERROR_TIMEOUT), NRF_STRERROR_ENTITY(NRF_ERROR_NULL),
	NRF_STRERROR_ENTITY(NRF_ERROR_FORBIDDEN), NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_ADDR),
	NRF_STRERROR_ENTITY(NRF_ERROR_BUSY),
#ifdef NRF_ERROR_CONN_COUNT
	NRF_STRERROR_ENTITY(NRF_ERROR_CONN_COUNT),
#endif
#ifdef NRF_ERROR_RESOURCES
	NRF_STRERROR_ENTITY(NRF_ERROR_RESOURCES),
#endif
};

const char *nrf_strerror_get(uint32_t code)
{
	const char *ret = nrf_strerror_find(code);

	return (ret == NULL) ? unknown_str : ret;
}

const char *nrf_strerror_find(uint32_t code)
{
	const struct strerror_desc *start;
	const struct strerror_desc *end;

	start = nrf_strerror_array;
	end = nrf_strerror_array + ARRAY_SIZE(nrf_strerror_array);

	while (start < end) {
		const struct strerror_desc *mid = start + ((end - start) / 2);
		uint32_t mid_c = mid->code;

		if (mid_c > code) {
			end = mid;
		} else if (mid_c < code) {
			start = mid + 1;
		} else {
			return mid->name;
		}
	}
	return NULL;
}
