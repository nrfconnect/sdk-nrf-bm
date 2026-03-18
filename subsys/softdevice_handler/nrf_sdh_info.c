/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <stdio.h>
#include <nrf_sdm.h>
#include <bm/softdevice_handler/nrf_sdh_info.h>

struct nrf_sdh_info_version nrf_sdh_info_version_get(uint32_t sd_base_addr)
{
	const uint32_t sd_ver = SD_VERSION_GET(sd_base_addr);
	struct nrf_sdh_info_version version;

	/* See the definition of SD_VERSION. */
	version.major = sd_ver / 1000000UL;
	version.minor = (sd_ver - (version.major * 1000000UL)) / 1000UL;
	version.bugfix = (sd_ver - (version.major * 1000000UL) - (version.minor * 1000UL));

	version.id = SD_ID_GET(sd_base_addr);
	version.fwid = SD_FWID_GET(sd_base_addr);

	return version;
}

struct nrf_sdh_info_unique_str nrf_sdh_info_unique_str_get(uint32_t sd_base_addr)
{
	int ret;
	unsigned int offset = 0;
	const uint8_t *addr = SD_UNIQUE_STR_ADDR_GET(sd_base_addr);
	struct nrf_sdh_info_unique_str buf;

	for (unsigned int i = 0; i < SD_UNIQUE_STR_SIZE; ++i) {
		ret = sprintf(&buf.str[offset], "%02x", addr[i]);
		if (ret < 0) {
			break;
		}

		offset += ret;
	}

	buf.str[offset++] = '\0';

	return buf;
}
