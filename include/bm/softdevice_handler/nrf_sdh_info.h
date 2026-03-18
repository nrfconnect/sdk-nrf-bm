/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup nrf_sdh_info SoftDevice Information utility in SoftDevice Handler
 * @{
 * @ingroup  nrf_sdh
 * @brief    Declarations of types and function for getting SoftDevice information structure values.
 */

#ifndef NRF_SDH_INFO_H__
#define NRF_SDH_INFO_H__

#include <stdint.h>
#include <nrf_sdm.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SoftDevice version info.
 */
struct nrf_sdh_info_version {
	/**
	 * @brief SoftDevice major version (sd_version).
	 */
	uint16_t major;
	/**
	 * @brief SoftDevice minor version (sd_version).
	 */
	uint16_t minor;
	/**
	 * @brief SoftDevice bugfix version (sd_version).
	 */
	uint16_t bugfix;
	/**
	 * @brief SoftDevice ID (sd_id).
	 */
	uint16_t id;
	/**
	 * @brief SoftDevice firmware ID (firmware_id).
	 */
	uint16_t fwid;
};

/**
 * @brief SoftDevice unique string.
 */
struct nrf_sdh_info_unique_str {
	/**
	 * @brief SoftDevice unique string (sd_unique_str) in string format.
	 */
	char str[2 * SD_UNIQUE_STR_SIZE + 1];
};

/**
 * @brief Get version information stored in the SoftDevice information structure.
 *
 * @param[in] sd_base_addr SoftDevice base address
 *
 * @return The SoftDevice version information.
 */
struct nrf_sdh_info_version nrf_sdh_info_version_get(uint32_t sd_base_addr);

/**
 * @brief Get the unique string stored in the SoftDevice information structure.
 *
 * @param[in] sd_base_addr SoftDevice base address.
 *
 * @return The SoftDevice unique string.
 */
struct nrf_sdh_info_unique_str nrf_sdh_info_unique_str_get(uint32_t sd_base_addr);

#ifdef __cplusplus
}
#endif

#endif /* NRF_SDH_INFO_H__ */

/** @} */
