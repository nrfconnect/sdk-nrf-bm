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
#include <nrfx_spim.h>
#include <board-config.h>

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
 * WREN (Write Enable) must precede any program or erase.
 * RDSR (Read Status Register). The Write In Progress (WIP) bit tells if an erase or program
 * operation has finished.
 */
#define MX25_CMD_PAGE_PROGRAM 0x02U
#define MX25_CMD_READ         0x03U
#define MX25_CMD_RDSR         0x05U
#define MX25_CMD_WREN         0x06U
#define MX25_CMD_SECTOR_ERASE 0x20U

#define MX25_CMD_HEADER_LEN   4U

/* Max payload bytes per MX25 Page Program command. */
#define MX25_PAGE_PROGRAM_LEN (255U - MX25_CMD_HEADER_LEN)

/* Status register bit: Write In Progress. Set while an erase/program runs internally. */
#define MX25_SR_WIP           0x01U

/* Expands to the 4 bytes of an MX25 command header: opcode + 24-bit big-endian address. */
#define MX25_CMD_HEADER(opcode, addr)								   \
	(opcode),										   \
	(uint8_t)((addr) >> 16),								   \
	(uint8_t)((addr) >> 8),									   \
	(uint8_t)(addr)

/* Single-byte WREN command. Required before any program or erase; shared by both. */
static uint8_t wren_cmd[1] = { MX25_CMD_WREN };

/* RDSR poll transaction, shared by flash_op_sent() and flash_op_done().
 * Sends the 1-byte MX25_CMD_RDSR opcode and reads back 2 bytes,
 * the status register lands in the 2nd RX byte (1st arrives while sending the opcode).
 */
static uint8_t rdsr_tx[1] = { MX25_CMD_RDSR };
static uint8_t rdsr_rx[2];
static const struct bm_spi_mngr_transfer rdsr_xfers[] = {
	BM_SPI_MNGR_TRANSFER(rdsr_tx, sizeof(rdsr_tx), rdsr_rx, sizeof(rdsr_rx)),
};
static void flash_op_done(int result, void *user_data);
static struct bm_spi_mngr_transaction rdsr_txn = {
	.transfers = rdsr_xfers,
	.number_of_transfers = ARRAY_SIZE(rdsr_xfers),
	.end_callback = flash_op_done,
	.required_spim_cfg = &spim_cfg_ext_mem,
};

/* True while an erase/program is scheduled or the chip is still busy (WIP bit set).
 * To avoid touching half-written sectors, refuse read/erase/program operations while this is set.
 * Marked volatile because it is written and read from interrupt context
 * (SPIM callbacks and button dispatch).
 */
static volatile bool flash_busy;

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

	BM_IRQ_DIRECT_CONNECT(
		NRFX_IRQ_NUMBER_GET(BOARD_EXTERNAL_MEMORY_SPIM_INST),
		IRQ_PRIO_LOWEST,
		spim_isr,
		0);
	irq_enable(NRFX_IRQ_NUMBER_GET(BOARD_EXTERNAL_MEMORY_SPIM_INST));

	return bm_spi_mngr_init(&spi_mgr, &spim_cfg_ext_mem);
}

/* End callback for each RDSR poll.
 * If the WIP bit is still set the flash is still erasing/writing, so queue another poll.
 * Once the WIP bit clears, the operation is done on the flash device. Unset the flash_busy flag.
 */
static void flash_op_done(int result, void *user_data)
{
	if (result != 0) {
		LOG_ERR("Status poll failed, err %d", result);
		/* Give up waiting rather than getting stuck busy forever. */
		flash_busy = false;
		return;
	}

	/* rdsr_rx[0] is received while we send the opcode, the real status byte arrives in [1]. */
	uint8_t write_in_progress = rdsr_rx[1] & MX25_SR_WIP;

	if (write_in_progress) {
		/* Still busy: queue another poll. */
		bm_spi_mngr_schedule(&spi_mgr, &rdsr_txn);
		return;
	}

	/* WIP bit clear: the operation has completed inside the flash. */
	flash_busy = false;

	LOG_INF("Flash operation complete");
}

/* End callback for each erase/program command.
 * The opcode has been sent over SPI, but the flash is still erasing/writing internally, so queue
 * a status register poll (MX25_CMD_RDSR) that flash_op_done repeats until the flash is done.
 */
static void flash_op_sent(int result, void *user_data)
{
	int err;

	if (result != 0) {
		LOG_ERR("Flash operation failed, err %d", result);
		/* Command never went out, so nothing is running, unset the busy flag. */
		flash_busy = false;
		return;
	}

	err = bm_spi_mngr_schedule(&spi_mgr, &rdsr_txn);
	if (err) {
		LOG_ERR("Failed to schedule status poll, err %d", err);
		/* Nothing will ever call flash_op_done to clear this, so drop it here. */
		flash_busy = false;
	}
}

static int flash_erase(void)
{
	static uint8_t erase_cmd[] = {
		MX25_CMD_HEADER(MX25_CMD_SECTOR_ERASE, CONFIG_SAMPLE_SPI_MNGR_FLASH_ADDR),
	};
	/* Two chip select (CS) frames are required:
	 * 1. WREN (MX25_CMD_WREN) sets the chip's Write Enable Latch (WEL) bit to allow an erase.
	 * 2. SECTOR_ERASE (MX25_CMD_SECTOR_ERASE) consumes the WEL bit to erase the sector.
	 *
	 * The SPI manager pulls the chip select line inactive between transfers,
	 * which is what sets the Write Enable Latch (WEL) bit.
	 */
	static const struct bm_spi_mngr_transfer erase_xfers[] = {
		BM_SPI_MNGR_TRANSFER(wren_cmd, sizeof(wren_cmd), NULL, 0),
		BM_SPI_MNGR_TRANSFER(erase_cmd, sizeof(erase_cmd), NULL, 0),
	};
	static struct bm_spi_mngr_transaction erase_txn = {
		.transfers = erase_xfers,
		.number_of_transfers = ARRAY_SIZE(erase_xfers),
		.end_callback = flash_op_sent,
		.required_spim_cfg = &spim_cfg_ext_mem,
	};

	/* Refuse to erase while an erase/program is still in progress. */
	if (flash_busy) {
		LOG_WRN("Flash busy, ignoring erase (wait for completion)");
		return -EBUSY;
	}

	flash_busy = true;

	LOG_INF("Erasing 4 KiB sector @ 0x%08lx", (unsigned long)CONFIG_SAMPLE_SPI_MNGR_FLASH_ADDR);

	return bm_spi_mngr_schedule(&spi_mgr, &erase_txn);
}

/* End callback for the read transaction.
 * The user_data argument points back to the read transaction,
 * which provides access to its RX buffer to read out the data received from the flash device.
 */
static void flash_read_done(int result, void *user_data)
{
	const uint8_t *read_rx = user_data;

	if (result != 0) {
		LOG_ERR("Flash read failed, err %d", result);
		return;
	}

	LOG_INF("Read %u bytes @ 0x%08lx:", (unsigned int)CONFIG_SAMPLE_SPI_MNGR_READ_LEN,
		(unsigned long)CONFIG_SAMPLE_SPI_MNGR_FLASH_ADDR);
	LOG_HEXDUMP_INF(&read_rx[MX25_CMD_HEADER_LEN], CONFIG_SAMPLE_SPI_MNGR_READ_LEN, "flash");
}

static int flash_read(void)
{
	/* Read buffer must outlive the queued transaction and is accessed by the read callback,
	 * so keep it static even though it is scoped to this function.
	 */
	static uint8_t read_tx[] = {
		MX25_CMD_HEADER(MX25_CMD_READ, CONFIG_SAMPLE_SPI_MNGR_FLASH_ADDR),
	};
	static uint8_t read_rx[MX25_CMD_HEADER_LEN + CONFIG_SAMPLE_SPI_MNGR_READ_LEN];
	static const struct bm_spi_mngr_transfer read_xfers[] = {
		BM_SPI_MNGR_TRANSFER(read_tx, sizeof(read_tx), read_rx, sizeof(read_rx)),
	};
	static struct bm_spi_mngr_transaction read_txn = {
		.transfers = read_xfers,
		.number_of_transfers = ARRAY_SIZE(read_xfers),
		.end_callback = flash_read_done,
		.required_spim_cfg = &spim_cfg_ext_mem,
	};

	/* Refuse to read while an erase/program is still in progress, otherwise there is a risk of
	 * reading a sector that is only partially erased or programmed.
	 */
	if (flash_busy) {
		LOG_WRN("Flash busy, ignoring read (wait for completion)");
		return -EBUSY;
	}

	/* Point user_data to the read_rx buffer, so the flash_read_done callback can reach it. */
	read_txn.user_data = &read_rx;

	return bm_spi_mngr_schedule(&spi_mgr, &read_txn);
}

static int flash_program(void)
{
	static uint8_t program_buf[MX25_CMD_HEADER_LEN + MX25_PAGE_PROGRAM_LEN] = {
		MX25_CMD_HEADER(MX25_CMD_PAGE_PROGRAM, CONFIG_SAMPLE_SPI_MNGR_FLASH_ADDR),
	};
	/* Two chip select (CS) frames are required:
	 * 1. WREN (MX25_CMD_WREN) sets the chip's Write Enable Latch (WEL) bit to allow a write.
	 * 2. PAGE_PROGRAM (MX25_CMD_PAGE_PROGRAM) consumes the WEL bit to program a page.
	 *
	 * The SPI manager pulls the chip select line inactive between transfers,
	 * which is what sets the Write Enable Latch (WEL) bit.
	 */
	static const struct bm_spi_mngr_transfer program_xfers[] = {
		BM_SPI_MNGR_TRANSFER(wren_cmd, sizeof(wren_cmd), NULL, 0),
		BM_SPI_MNGR_TRANSFER(program_buf, sizeof(program_buf), NULL, 0),
	};
	static struct bm_spi_mngr_transaction program_txn = {
		.transfers = program_xfers,
		.number_of_transfers = ARRAY_SIZE(program_xfers),
		.end_callback = flash_op_sent,
		.required_spim_cfg = &spim_cfg_ext_mem,
	};

	/* Refuse to program while an erase/program is still in progress, otherwise the
	 * WREN/PP would be issued mid-operation and ignored (or corrupt the sector).
	 */
	if (flash_busy) {
		LOG_WRN("Flash busy, ignoring program (wait for completion)");
		return -EBUSY;
	}

	size_t len = strlen(CONFIG_SAMPLE_SPI_MNGR_MSG);

	if (len > MX25_PAGE_PROGRAM_LEN) {
		len = MX25_PAGE_PROGRAM_LEN;
	}

	/* Pad unused payload bytes with 0xFF so NOR flash leaves them unchanged.
	 * This means that only message bytes get programmed.
	 */
	memset(&program_buf[MX25_CMD_HEADER_LEN], 0xFF, MX25_PAGE_PROGRAM_LEN);
	memcpy(&program_buf[MX25_CMD_HEADER_LEN], CONFIG_SAMPLE_SPI_MNGR_MSG, len);

	LOG_INF("Programming \"%s\" @ 0x%08lx", CONFIG_SAMPLE_SPI_MNGR_MSG,
		(unsigned long)CONFIG_SAMPLE_SPI_MNGR_FLASH_ADDR);

	flash_busy = true;

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
		LOG_ERR("SPI init failed, err %d", err);
		goto idle;
	}

	err = buttons_init();
	if (err) {
		LOG_ERR("Buttons init failed, err %d", err);
		goto idle;
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);

	LOG_INF("SPI manager sample initialized");
	LOG_INF("Use following buttons to interact with external memory through SPI");
	LOG_INF("Button 1: Erase");
	LOG_INF("Button 2: Read");
	LOG_INF("Button 3: Program");

	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);

idle:
	while (true) {
		log_flush();
		k_cpu_idle();
	}
}
