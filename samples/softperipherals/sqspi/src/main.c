/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/ring_buffer.h>

#include <bm/bm_buttons.h>
#include <bm/bm_irq.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_spu.h>
#include <nrf_sqspi.h>
#include <softperipheral_regif.h>

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_SQSPI_LOG_LEVEL);

/* MX25R6435F opcodes:
 * WRSR (Write Status Register) sets the Quad Enable (QE) bit for the 4-line commands.
 * RDSR (Read Status Register). The Write In Progress (WIP) bit tells if an erase or program
 * operation has finished.
 * WREN (Write Enable) must precede WRSR, any program or erase.
 * SECTOR ERASE, 4PAGE PROGRAM and 4READ all use the same header: 1 opcode byte + 3 address bytes.
 * 4PAGE PROGRAM and 4READ send the address and data on 4 lines.
 */
#define MX25_CMD_WRSR	       0x01U
#define MX25_CMD_RDSR	       0x05U
#define MX25_CMD_WREN	       0x06U
#define MX25_CMD_SECTOR_ERASE  0x20U
#define MX25_CMD_4PAGE_PROGRAM 0x38U
#define MX25_CMD_4READ	       0xEBU

/* Command header lengths, in bits: 1 opcode byte + 3 address bytes. */
#define MX25_CMD_LEN	 8
#define MX25_ADDRESS_LEN 24

/* Status register bits:
 * WIP (Write In Progress) is set while an erase or program runs inside the flash.
 * WEL (Write Enable Latch) is set by WREN and cleared after each write.
 * QE (Quad Enable) must be set to use the 4-line commands.
 */
#define MX25_WIP_BIT    (1 << 0)
#define MX25_WEL_BIT    (1 << 1)
#define MX25_QUAD_ENABLE_BIT (1 << 6)

static volatile uint8_t read_buf[sizeof(CONFIG_SAMPLE_SQSPI_MSG)];
static uint8_t quad_enable[3] = { MX25_QUAD_ENABLE_BIT, 0, 0 };

/* Set to false if no command in progress. */
static atomic_t xfer_in_progress;
struct xfer {
	nrf_sqspi_xfer_t sqspi_xfer;
	nrf_sqspi_spi_lines_t sqspi_lines;
	uint8_t sr_mask;
	uint8_t sr_state;
	bool last_in_cmd;
};
struct xfer current_xfer;

static volatile uint8_t status_reg;
RING_BUF_DECLARE(op_queue, sizeof(struct xfer) * 16);

/* The sQSPI registers are in RAM shared with the FLPR core that runs the soft peripheral. */
static nrf_sqspi_t qspi = {
	.p_reg = (void *)DT_REG_ADDR(DT_NODELABEL(flpr_vri_ram)),
	.drv_inst_idx = 0
};

static void sqspi_xfer_done_cb(nrf_sqspi_t const *qspi, nrf_sqspi_evt_t *evt, void *ctx);

ISR_DIRECT_DECLARE(sqspi_direct_isr)
{
	nrf_sqspi_irq_handler();

	return 0;
}

static int op_queue_pop(struct xfer *xfer)
{
	int err = 0;
	uint32_t rc;
	unsigned int key;

	key = irq_lock();

	rc = ring_buf_get(&op_queue, (uint8_t *)xfer, sizeof(struct xfer));
	if (rc != sizeof(struct xfer)) {
		err = -ENOENT;
	}

	irq_unlock(key);
	return err;
}

static int op_queue_push(const struct xfer xfer[], size_t num_xfers)
{
	uint32_t err = 0;
	unsigned int key;

	key = irq_lock();

	if (ring_buf_space_get(&op_queue) < (sizeof(struct xfer) * num_xfers)) {
		err = -ENOMEM;
		goto out;
	}

	for (int i = 0; i < num_xfers; i++) {
		ring_buf_put(&op_queue, (uint8_t *)&xfer[i], sizeof(struct xfer));
	}

out:
	irq_unlock(key);
	return err;
}

/* Abort entire command including all its transfers. */
static void sqspi_cmd_abort(void)
{
	unsigned int key;
	bool cmd_done = current_xfer.last_in_cmd;
	struct xfer skipped;
	uint32_t len;

	key = irq_lock();

	/* Remove queued transfers up to and including the last one of the failed command. */
	while (!cmd_done) {
		len = ring_buf_get(&op_queue, (uint8_t *)&skipped, sizeof(skipped));
		if (len == 0) {
			/* The queue is empty. */
			break;
		}

		cmd_done = skipped.last_in_cmd;
	}

	atomic_set(&xfer_in_progress, 0);
	irq_unlock(key);
}

static int sqspi_cmd_xfer_next(nrf_sqspi_t const *qspi)
{
	int err;
	struct xfer xfer;
	nrf_sqspi_dev_cfg_t dev_cfg = {
		.csn_pin = BOARD_EXTERNAL_MEMORY_SPIM_PIN_CSN,
		.sck_freq_khz = 1000,
		.spi_cpolpha = NRF_SQSPI_SPI_CPOLPHA_0,
		.mspi_ddr = NRF_SQSPI_SPI_DDR_SINGLE,
	};

	if (!atomic_cas(&xfer_in_progress, 0, 1)) {
		return -EINPROGRESS;
	}

	err = op_queue_pop(&xfer);
	if (err) {
		goto err;
	}

	dev_cfg.mspi_lines = xfer.sqspi_lines;

	err = nrf_sqspi_dev_cfg(qspi, &dev_cfg, sqspi_xfer_done_cb, NULL);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrf_sqspi_dev_cfg failed, err #%x", err);
		err = -EIO;
		goto err;
	}

	current_xfer = xfer;

	if ((xfer.sqspi_xfer.cmd == MX25_CMD_SECTOR_ERASE ||
	    xfer.sqspi_xfer.cmd == MX25_CMD_4PAGE_PROGRAM) &&
	    !(status_reg & MX25_WEL_BIT)) {
		LOG_WRN("Write/erase operation scheduled with WEL bit not set");
	}

	err = nrf_sqspi_xfer(qspi, &xfer.sqspi_xfer, 1, 0);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrf_sqspi_xfer failed, err #%x", err);
		err = -EIO;
		goto err;
	}

	return 0;

err:
	atomic_set(&xfer_in_progress, 0);
	sqspi_cmd_abort();
	return err;
}

static void sqspi_xfer_done_cb(nrf_sqspi_t const *qspi, nrf_sqspi_evt_t *evt, void *ctx)
{
	int err;

	if (evt->type != NRF_SQSPI_EVT_XFER_DONE) {
		/* Only process transfer done events.*/
		return;
	}

	if (evt->data.xfer_done != NRF_SQSPI_RESULT_OK) {
		LOG_ERR("Transfer done error, err = %d", evt->data.xfer_done);
		sqspi_cmd_abort();
		sqspi_cmd_xfer_next(qspi);
		return;
	}

	switch (current_xfer.sqspi_xfer.cmd) {
	case MX25_CMD_4READ:
		LOG_HEXDUMP_INF((uint8_t *)read_buf, sizeof(read_buf), "Read complete");
		break;
	case MX25_CMD_SECTOR_ERASE:
		LOG_INF("Erase complete");
		break;
	case MX25_CMD_4PAGE_PROGRAM:
		LOG_INF("Write complete");
		break;
	}

	/* Operation complete, no active command. */
	atomic_set(&xfer_in_progress, 0);

	if ((status_reg & current_xfer.sr_mask) != current_xfer.sr_state) {
		/* Wait for internal operation to complete, schedule last operation again. */

		err = nrf_sqspi_xfer(qspi, &current_xfer.sqspi_xfer, 1, 0);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("Failed to read status register, err = %d", err);
		}

		return;
	}

	(void)sqspi_cmd_xfer_next(qspi);
}

static int mx25_cmd_simple(nrf_sqspi_spi_lines_t lines, const nrf_sqspi_xfer_t *xfer)
{
	struct xfer cmd = {
		.sqspi_xfer = *xfer,
		.sqspi_lines = lines,
		.last_in_cmd = true,
	};

	return op_queue_push(&cmd, 1);
}

static int mx25_cmd_steps(nrf_sqspi_spi_lines_t lines, const nrf_sqspi_xfer_t *xfer)
{
	const struct xfer steps[] = {
		{ /* Write enable. */
			.sqspi_xfer = {
				.dir = NRF_SQSPI_XFER_DIR_TX,
				.cmd = MX25_CMD_WREN,
				.cmd_length = MX25_CMD_LEN,
			},
			.sqspi_lines = NRF_SQSPI_SPI_LINES_SINGLE,
		},
		{ /* Wait for write enable latch is set. */
			.sqspi_xfer = {
				.dir = NRF_SQSPI_XFER_DIR_RX,
				.cmd = MX25_CMD_RDSR,
				.cmd_length = MX25_CMD_LEN,
				.p_data = (void *)&status_reg,
				.data_length = sizeof(status_reg),
			},
			.sqspi_lines = NRF_SQSPI_SPI_LINES_SINGLE,
			.sr_mask = MX25_WEL_BIT | MX25_WIP_BIT,
			.sr_state = MX25_WEL_BIT, /* Write enable latch set. */
		},
		{ /* Write / erase */
			.sqspi_xfer = *xfer,
			.sqspi_lines = lines,
		},
		{ /* Wait for command to complete. */
			.sqspi_xfer = {
				.dir = NRF_SQSPI_XFER_DIR_RX,
				.cmd = MX25_CMD_RDSR,
				.cmd_length = MX25_CMD_LEN,
				.p_data = (void *)&status_reg,
				.data_length = sizeof(status_reg),
			},
			.sqspi_lines = NRF_SQSPI_SPI_LINES_SINGLE,
			.sr_mask = MX25_WIP_BIT,
			.sr_state = 0, /* Write not in progress. */
			.last_in_cmd = true,
		},
	};

	return op_queue_push(steps, ARRAY_SIZE(steps));
}

static int mx25_cmd(nrf_sqspi_spi_lines_t lines, const nrf_sqspi_xfer_t *xfer)
{
	if (xfer->dir == NRF_SQSPI_XFER_DIR_RX) {
		return mx25_cmd_simple(lines, xfer);
	}

	return mx25_cmd_steps(lines, xfer);
}

static int external_memory_init(nrf_sqspi_t const *qspi)
{
	int err;
	/* Quad commands only work when the Quad Enable (QE) bit in the status register is set. */
	const nrf_gpio_pin_drive_t drive = NRF_GPIO_PIN_E0E1;
	static const nrf_sqspi_cfg_t cfg = {
		.pins = {
			.sck = BOARD_EXTERNAL_MEMORY_SPIM_PIN_SCK,
			.strobe = NRF_SQSPI_PINS_UNUSED,
			.io = {
				BOARD_EXTERNAL_MEMORY_SPIM_PIN_MOSI,
				BOARD_EXTERNAL_MEMORY_SPIM_PIN_MISO,
				BOARD_EXTERNAL_MEMORY_PIN_WP,
				BOARD_EXTERNAL_MEMORY_PIN_RST,
			},
		},
	};
	nrf_sqspi_data_fmt_t sqspi_data_fmt = {
		.cmd_bit_order = NRF_SQSPI_DATA_FMT_BIT_ORDER_MSB_FIRST,
		.addr_bit_order = NRF_SQSPI_DATA_FMT_BIT_ORDER_MSB_FIRST,
		.data_bit_order = NRF_SQSPI_DATA_FMT_BIT_ORDER_MSB_FIRST,
		.data_bit_reorder_unit = 8,
		.data_container = 32,
		.data_swap_unit = 8,
		.data_padding = NRF_SQSPI_DATA_FMT_PAD_RAW,
	};
	nrf_sqspi_xfer_t sqspi_xfers[] = {
		{	/* Write status register */
			.dir = NRF_SQSPI_XFER_DIR_TX,
			.cmd = MX25_CMD_WRSR,
			.cmd_length = MX25_CMD_LEN,
			.p_data = quad_enable,
			.data_length = sizeof(quad_enable),
		},
	};

	/* FLPR must be a secure bus master to access its RAM and MEMCONF. */
	nrf_spu_periph_perm_secattr_set(NRF_SPU00, nrf_address_slave_get((uint32_t)NRF_VPR), true);

	err = nrf_sqspi_init(qspi, &cfg);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrf_sqspi_init failed, err #%x", err);
		return -EIO;
	}

	/* The driver uses standard drive, which is too weak for quad reads. */
	nrf_gpio_reconfigure(cfg.pins.sck, NULL, NULL, NULL, &drive, NULL);
	for (size_t i = 0; i < ARRAY_SIZE(cfg.pins.io); i++) {
		nrf_gpio_reconfigure(cfg.pins.io[i], NULL, NULL, NULL, &drive, NULL);
	}

	err = nrf_sqspi_dev_data_fmt_set(qspi, &sqspi_data_fmt);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrf_sqspi_dev_data_fmt_set() failed: %d", err);
		return -EIO;
	}

	BM_IRQ_DIRECT_CONNECT(SP_VPR_IRQn, 2, sqspi_direct_isr, 0);

	err = nrf_sqspi_activate(qspi);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrf_sqspi_activate failed, err = %d", err);
		return -EIO;
	}

	err = mx25_cmd(NRF_SQSPI_SPI_LINES_SINGLE, sqspi_xfers);
	if (err) {
		LOG_ERR("mx25_cmd failed, err = %d", err);
		return err;
	}

	return 0;
}

static int flash_erase(uint64_t addr, size_t addr_len)
{
	int err;
	nrf_sqspi_xfer_t sqspi_xfers[] = {
		{	/* Erase */
			.dir = NRF_SQSPI_XFER_DIR_TX,
			.cmd = MX25_CMD_SECTOR_ERASE,
			.cmd_length = MX25_CMD_LEN,
			.address = addr,
			.addr_length = addr_len,
		},
	};

	err = mx25_cmd(NRF_SQSPI_SPI_LINES_SINGLE, sqspi_xfers);
	if (err) {
		LOG_ERR("mx25_cmd failed err = %d", err);
		return err;
	}

	LOG_DBG("Pushed erase to queue for address #%llx", addr);

	return 0;
}

static int flash_read(uint64_t addr, size_t addr_len, void *buf, size_t buf_len)
{
	int err;
	nrf_sqspi_xfer_t sqspi_xfers[] = {
		{	/* Read */
			.dir = NRF_SQSPI_XFER_DIR_RX,
			.cmd = MX25_CMD_4READ,
			.cmd_length = MX25_CMD_LEN,
			.address = addr,
			.addr_length = addr_len,
			.dummy_length = 6,
			.p_data = buf,
			.data_length = buf_len,
		},
	};

	err = mx25_cmd(NRF_SQSPI_SPI_LINES_QUAD_1_4_4, sqspi_xfers);
	if (err) {
		LOG_ERR("mx25_cmd failed err = %d", err);
		return err;
	}

	LOG_DBG("Pushed read to queue for address #%llx", addr);

	return 0;
}

static int flash_write(uint64_t addr, size_t addr_len, void *data, size_t data_len)
{
	int err;
	nrf_sqspi_xfer_t sqspi_xfers[] = {
		{	/* Page program */
			.dir = NRF_SQSPI_XFER_DIR_TX,
			.cmd = MX25_CMD_4PAGE_PROGRAM,
			.cmd_length = MX25_CMD_LEN,
			.address = addr,
			.addr_length = addr_len,
			.p_data = data,
			.data_length = data_len,
		},
	};

	err = mx25_cmd(NRF_SQSPI_SPI_LINES_QUAD_1_4_4, sqspi_xfers);
	if (err) {
		LOG_ERR("mx25_cmd failed err = %d", err);
		return err;
	}

	LOG_DBG("Pushed page program to queue for address #%llx", addr);

	return 0;
}

static void button_handler(uint8_t pin, enum bm_buttons_evt_type action)
{
	int err;

	if (action != BM_BUTTONS_PRESS) {
		return;
	}

	switch (pin) {
	case BOARD_PIN_BTN_1:
		LOG_INF("Erasing...");
		err = flash_erase(CONFIG_SAMPLE_SQSPI_FLASH_ADDR, MX25_ADDRESS_LEN);
		if (err) {
			LOG_ERR("Flash erase failed, err = %d", err);
		}
		break;
	case BOARD_PIN_BTN_2:
		LOG_INF("Reading...");
		err = flash_read(CONFIG_SAMPLE_SQSPI_FLASH_ADDR, MX25_ADDRESS_LEN,
				 (void *)read_buf, sizeof(read_buf));
		if (err) {
			LOG_ERR("Flash read failed, err = %d", err);
		}
		break;
	case BOARD_PIN_BTN_3:
		LOG_INF("Writing...");
		err = flash_write(CONFIG_SAMPLE_SQSPI_FLASH_ADDR, MX25_ADDRESS_LEN,
				  CONFIG_SAMPLE_SQSPI_MSG, sizeof(CONFIG_SAMPLE_SQSPI_MSG));
		if (err) {
			LOG_ERR("Flash program failed, err = %d", err);
		}
		break;
	default:
		break;
	}

	if (!atomic_get(&xfer_in_progress)) {
		(void)sqspi_cmd_xfer_next(&qspi);
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

	err = buttons_init();
	if (err) {
		LOG_ERR("Buttons init failed, err %d", err);
		goto idle;
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);

	err = external_memory_init(&qspi);
	if (err) {
		LOG_ERR("SPI init failed, err #%x", err);
		goto idle;
	}

	LOG_INF("sQSPI sample initialized");
	LOG_INF("Use the following buttons to interact with the external memory");
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
