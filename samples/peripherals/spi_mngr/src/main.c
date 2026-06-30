/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <bm/bm_buttons.h>
#include <bm/bm_irq.h>
#include <bm/bm_spi_mngr.h>
#include <hal/nrf_gpio.h>
#include <board-config.h>
#include <nrfx_spim.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_SPI_MNGR_LOG_LEVEL);

BM_SPI_MNGR_DEF(spi_mgr, 4, BOARD_EXTERNAL_MEMORY_SPIM_INST);

/* SPIM configuration for the external memory:
 * You can define several configurations and point each transaction at one,
 * so the same SPIM can talk to different devices (pins) as you queue up work.
 */
static nrfx_spim_config_t spim_cfg_ext_mem = NRFX_SPIM_DEFAULT_CONFIG(
	BOARD_EXTERNAL_MEMORY_SPIM_PIN_SCK, BOARD_EXTERNAL_MEMORY_SPIM_PIN_MOSI,
	BOARD_EXTERNAL_MEMORY_SPIM_PIN_MISO, BOARD_EXTERNAL_MEMORY_SPIM_PIN_CSN);

/* MX25R6435F opcodes:
 * READ, PAGE PROGRAM and SECTOR ERASE all use the same header: 1 opcode byte + 3 address bytes.
 * WREN must precede any program or erase.
 */
#define MX25_CMD_WREN         0x06U
#define MX25_CMD_READ         0x03U
#define MX25_CMD_PAGE_PROGRAM 0x02U
#define MX25_CMD_SECTOR_ERASE 0x20U

#define MX25_CMD_HEADER_LEN   4U
#define FLASH_ADDR            0x00100000UL
#define BLOCK_LEN             64U

/* Expands to the 4 bytes of an MX25 command header: opcode + 24-bit big-endian address. */
#define MX25_CMD_HEADER(opcode, addr)								   \
	(opcode),										   \
	(uint8_t)((addr) >> 16),								   \
	(uint8_t)((addr) >> 8),									   \
	(uint8_t)(addr)

/* Read buffer must outlive the queued transaction and is accessed by the read callback. */
static uint8_t read_rx[MX25_CMD_HEADER_LEN + BLOCK_LEN];

/* Single-byte WREN command. Required before any program or erase; shared by both. */
static uint8_t wren_cmd[1] = { MX25_CMD_WREN };

/* Drive WP# and RST# high so the chip is writable and not in reset. */
static void mx25_straps_high(void)
{
	nrf_gpio_cfg_output(BOARD_EXTERNAL_MEMORY_PIN_WP);
	nrf_gpio_cfg_output(BOARD_EXTERNAL_MEMORY_PIN_RST);
	nrf_gpio_pin_write(BOARD_EXTERNAL_MEMORY_PIN_WP, 1);
	nrf_gpio_pin_write(BOARD_EXTERNAL_MEMORY_PIN_RST, 1);
}

ISR_DIRECT_DECLARE(spim_isr)
{
	nrfx_spim_irq_handler(spi_mgr.spim);
	return 0;
}

static int external_memory_init(void)
{
	mx25_straps_high();

	BM_IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(BOARD_EXTERNAL_MEMORY_SPIM_INST), IRQ_PRIO_LOWEST,
			      spim_isr, 0);
	irq_enable(NRFX_IRQ_NUMBER_GET(BOARD_EXTERNAL_MEMORY_SPIM_INST));

	return bm_spi_mngr_init(&spi_mgr, &spim_cfg_ext_mem);
}

static int flash_erase(void)
{
	static uint8_t erase_cmd[] = {
		MX25_CMD_HEADER(MX25_CMD_SECTOR_ERASE, FLASH_ADDR),
	};
	/* Two CS frames are required: the first sends WREN (MX25_CMD_WREN) which
	 * sets the chips Write Enable Latch (WEL) bit, and the second sends SE
	 * (MX25_CMD_SECTOR_ERASE) which consumes the WEL bit to authorize the erase.
	 * The SPI manager deasserts CS between transfers, which is what latches WEL.
	 */
	static const struct bm_spi_mngr_transfer erase_xfers[] = {
		BM_SPI_MNGR_TRANSFER(wren_cmd, sizeof(wren_cmd), NULL, 0),
		BM_SPI_MNGR_TRANSFER(erase_cmd, sizeof(erase_cmd), NULL, 0),
	};
	static struct bm_spi_mngr_transaction erase_txn = {
		.transfers = erase_xfers,
		.number_of_transfers = ARRAY_SIZE(erase_xfers),
		.required_spim_cfg = &spim_cfg_ext_mem,
	};

	LOG_INF("Erasing 4 KiB sector @ 0x%08lx", (unsigned long)FLASH_ADDR);

	return bm_spi_mngr_schedule(&spi_mgr, &erase_txn);
}

static void flash_read_done(int result, void *user_data)
{
	if (result != 0) {
		LOG_ERR("Flash read failed: %d", result);
		return;
	}

	LOG_INF("Read (0x03) @ 0x%08lx, %u bytes:", (unsigned long)FLASH_ADDR,
		(unsigned int)BLOCK_LEN);
	LOG_HEXDUMP_INF(&read_rx[MX25_CMD_HEADER_LEN], BLOCK_LEN, "flash");
}

static int flash_read(void)
{
	static uint8_t read_tx[] = {
		MX25_CMD_HEADER(MX25_CMD_READ, FLASH_ADDR),
	};
	static const struct bm_spi_mngr_transfer read_xfers[] = {
		BM_SPI_MNGR_TRANSFER(read_tx, sizeof(read_tx), read_rx, sizeof(read_rx)),
	};
	static struct bm_spi_mngr_transaction read_txn = {
		.transfers = read_xfers,
		.number_of_transfers = ARRAY_SIZE(read_xfers),
		.end_callback = flash_read_done,
		.required_spim_cfg = &spim_cfg_ext_mem,
	};

	return bm_spi_mngr_schedule(&spi_mgr, &read_txn);
}

static int flash_program(void)
{
	static uint8_t program_buf[MX25_CMD_HEADER_LEN + BLOCK_LEN] = {
		MX25_CMD_HEADER(MX25_CMD_PAGE_PROGRAM, FLASH_ADDR),
	};
	/* Two CS frames are required: the first sends WREN (MX25_CMD_WREN) which
	 * sets the chip's Write Enable Latch (WEL) bit, and the second sends PP
	 * (MX25_CMD_PAGE_PROGRAM) which consumes the WEL bit to authorize the write.
	 * The SPI manager deasserts CS between transfers, which is what latches WEL.
	 */
	static const struct bm_spi_mngr_transfer program_xfers[] = {
		BM_SPI_MNGR_TRANSFER(wren_cmd, sizeof(wren_cmd), NULL, 0),
		BM_SPI_MNGR_TRANSFER(program_buf, sizeof(program_buf), NULL, 0),
	};
	static struct bm_spi_mngr_transaction program_txn = {
		.transfers = program_xfers,
		.number_of_transfers = ARRAY_SIZE(program_xfers),
		.required_spim_cfg = &spim_cfg_ext_mem,
	};

	size_t len = strlen(CONFIG_SAMPLE_SPI_MNGR_MSG);

	if (len > BLOCK_LEN) {
		len = BLOCK_LEN;
	}

	/* Pad unused payload bytes with 0xFF so NOR flash leaves them unchanged.
	 * This means that only message bytes get programmed.
	 */
	memset(&program_buf[MX25_CMD_HEADER_LEN], 0xFF, BLOCK_LEN);
	memcpy(&program_buf[MX25_CMD_HEADER_LEN], CONFIG_SAMPLE_SPI_MNGR_MSG, len);

	LOG_INF("Programming \"%s\" @ 0x%08lx", CONFIG_SAMPLE_SPI_MNGR_MSG,
		(unsigned long)FLASH_ADDR);

	return bm_spi_mngr_schedule(&spi_mgr, &program_txn);
}

static void button_handler(uint8_t pin, enum bm_buttons_evt_type action)
{
	if (action != BM_BUTTONS_PRESS) {
		return;
	}

	switch (pin) {
	case BOARD_PIN_BTN_1:
		(void)flash_erase();
		break;
	case BOARD_PIN_BTN_2:
		(void)flash_read();
		break;
	case BOARD_PIN_BTN_3:
		(void)flash_program();
		break;
	default:
		break;
	}
}

static int buttons_init(void)
{
	int err;

	static const struct bm_buttons_config cfg[] = {
		{
			.pin_number = BOARD_PIN_BTN_1,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_2,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_3,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
	};

	err = bm_buttons_init(cfg, ARRAY_SIZE(cfg), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		return err;
	}

	return bm_buttons_enable();
}

int main(void)
{
	int err;

	err = external_memory_init();
	if (err) {
		LOG_ERR("SPI init failed: %d", err);
		goto idle;
	}

	err = buttons_init();
	if (err) {
		LOG_ERR("Buttons init failed: %d", err);
		goto idle;
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);

	LOG_INF("SPI manager sample initialized");
	LOG_INF("Use following buttons to interact with external memory through SPI");
	LOG_INF("Button 1: Erase");
	LOG_INF("Button 2: Read");
	LOG_INF("Button 3: Program");

	/* LED on once init succeeds. */
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);

idle:
	while (true) {
		log_flush();
		k_cpu_idle();
	}
}
