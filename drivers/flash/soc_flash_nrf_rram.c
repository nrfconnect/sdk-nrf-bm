/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nordic_rram_controller

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <zephyr/sys/barrier.h>
#include <hal/nrf_rramc.h>
#include <nrf_soc.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>

/* Note that it is supported to compile this driver for both secure
 * and non-secure images, but non-secure images cannot call
 * nrf_rramc_config_set because NRF_RRAMC_NS does not exist.
 *
 * Instead, when TF-M boots, it will configure RRAMC with this static
 * configuration:
 *
 * nrf_rramc_config_t config = {
 *   .mode_write = true,
 *   .write_buff_size = WRITE_BUFFER_SIZE
 * };
 *
 * nrf_rramc_ready_next_timeout_t params = {
 *   .value = CONFIG_NRF_RRAM_READYNEXT_TIMEOUT_VALUE,
 *   .enable = true,
 * };
 *
 * For more details see NCSDK-26982.
 */

LOG_MODULE_REGISTER(flash_nrf_rram, CONFIG_FLASH_LOG_LEVEL);

#define RRAM DT_INST(0, soc_nv_flash)

#if defined(CONFIG_SOC_SERIES_BSIM_NRFXX)
#define RRAM_START NRF_RRAM_BASE_ADDR
#else
#define RRAM_START DT_REG_ADDR(RRAM)
#endif
#define RRAM_SIZE  DT_REG_SIZE(RRAM)

#define PAGE_SIZE  DT_PROP(RRAM, erase_block_size)
#define PAGE_COUNT ((RRAM_SIZE) / (PAGE_SIZE))

#define WRITE_BLOCK_SIZE_FROM_DT DT_PROP(RRAM, write_block_size)
#define ERASE_VALUE              0xFF
#define WRITE_LINE_SIZE       WRITE_BLOCK_SIZE_FROM_DT

static inline bool is_aligned_32(uint32_t data)
{
	return (data & 0x3) ? false : true;
}

static inline bool is_within_bounds(off_t addr, size_t len, off_t boundary_start,
				    size_t boundary_size)
{
	return (addr >= boundary_start && (addr < (boundary_start + boundary_size)) &&
		(len <= (boundary_start + boundary_size - addr)));
}

static int nrf_rram_read(const struct device *dev, off_t addr, void *data, size_t len)
{
	ARG_UNUSED(dev);

	if (!is_within_bounds(addr, len, 0, RRAM_SIZE)) {
		return -EINVAL;
	}
	addr += RRAM_START;

#if CONFIG_TRUSTED_EXECUTION_NONSECURE && USE_PARTITION_MANAGER && PM_APP_ADDRESS
	if (addr < PM_APP_ADDRESS) {
		return soc_secure_mem_read(data, (void *)addr, len);
	}
#endif

	memcpy(data, (void *)addr, len);

	return 0;
}

static int nrf_rram_write(const struct device *dev, off_t addr, const void *data, size_t len)
{
	int ret = 0;

	ARG_UNUSED(dev);

	if (data == NULL) {
		return -EINVAL;
	}

	if (!is_within_bounds(addr, len, 0, RRAM_SIZE)) {
		return -EINVAL;
	}

	addr += RRAM_START;

	if (!is_aligned_32(addr) || (len % sizeof(uint32_t))) {
		LOG_ERR("Not word-aligned: 0x%08lx:%zu", (unsigned long)addr, len);
		return -EINVAL;
	}

	if (!len) {
		return 0;
	}

	LOG_DBG("Write: %p: %zu", (void *)addr, len);

	ret = sd_flash_write((uint32_t *)addr, data, len / sizeof(uint32_t));

	if (ret) {
		return -EIO;
	}

	while (1) {
		int taskid;

		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();

		ret = sd_evt_get(&taskid);

		if (!ret && (taskid == NRF_EVT_FLASH_OPERATION_SUCCESS ||
			     taskid == NRF_EVT_FLASH_OPERATION_ERROR)) {
			if (taskid != NRF_EVT_FLASH_OPERATION_SUCCESS) {
				ret = -EIO;
			}

			break;
		}
	}

	barrier_dmem_fence_full(); /* Barrier following our last write. */

	return ret;
}

static int nrf_rram_erase(const struct device *dev, off_t addr, size_t len)
{
	int ret = 0;
	uint8_t dummy_buf[16];

	memset(dummy_buf, ERASE_VALUE, sizeof(dummy_buf));

	if (!is_aligned_32(addr) || (len % sizeof(uint32_t))) {
		LOG_ERR("Not word-aligned: 0x%08lx:%zu", (unsigned long)addr, len);
		return -EINVAL;
	}

	while (len) {
		uint8_t erase_len = (len < sizeof(dummy_buf) ? len : sizeof(dummy_buf));

		ret = nrf_rram_write(dev, addr, dummy_buf, erase_len);

		if (ret) {
			break;
		}

		len -= erase_len;
		addr += erase_len;
	}

	barrier_dmem_fence_full(); /* Barrier following our last write. */

	return ret;
}

int nrf_rram_get_size(const struct device *dev, uint64_t *size)
{
	ARG_UNUSED(dev);

	*size = RRAM_SIZE;

	return 0;
}

static const struct flash_parameters *nrf_rram_get_parameters(const struct device *dev)
{
	ARG_UNUSED(dev);

	static const struct flash_parameters parameters = {
		.write_block_size = WRITE_LINE_SIZE,
		.erase_value = ERASE_VALUE,
		.caps = {
			.no_explicit_erase = true,
		},
	};

	return &parameters;
}

#if defined(CONFIG_FLASH_PAGE_LAYOUT)
static void nrf_rram_page_layout(const struct device *dev,
				 const struct flash_pages_layout **layout,
				 size_t *layout_size)
{
	ARG_UNUSED(dev);

	static const struct flash_pages_layout pages_layout = {
		.pages_count = PAGE_COUNT,
		.pages_size = PAGE_SIZE,
	};

	*layout = &pages_layout;
	*layout_size = 1;
}
#endif

static DEVICE_API(flash, nrf_rram_api) = {
	.read = nrf_rram_read,
	.write = nrf_rram_write,
	.erase = nrf_rram_erase,
	.get_size = nrf_rram_get_size,
	.get_parameters = nrf_rram_get_parameters,
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	.page_layout = nrf_rram_page_layout,
#endif
};

static int nrf_rram_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

DEVICE_DT_INST_DEFINE(0, nrf_rram_init, NULL, NULL, NULL, POST_KERNEL, CONFIG_FLASH_INIT_PRIORITY,
		      &nrf_rram_api);
