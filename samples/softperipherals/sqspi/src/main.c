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
#define MX25_WIP_BIT		    (1 << 0)
#define MX25_WEL_BIT		    (1 << 1)
#define MX25_QUAD_ENABLE_BIT_OFFSET 6

/* Status bit states for mx25_wait(). */
#define MX25_BIT_CLEARED false
#define MX25_BIT_SET	 true

RING_BUF_DECLARE(evt_queue, sizeof(nrf_sqspi_xfer_t) * 32);

static uint8_t read_rx[sizeof(CONFIG_SAMPLE_SQSPI_MSG)];
static const uint8_t quad_enable[3] = {1 << MX25_QUAD_ENABLE_BIT_OFFSET, 0, 0};
static volatile uint8_t status_reg;

static uint32_t cmd;

static const nrf_sqspi_xfer_t xfers[] = {
	{	/* Read */
		.dir = NRF_SQSPI_XFER_DIR_RX,
		.cmd = MX25_CMD_4READ,
		.cmd_length = MX25_CMD_LEN,
		.address = CONFIG_SAMPLE_SQSPI_FLASH_ADDR,
		.addr_length = MX25_ADDRESS_LEN,
		.dummy_length = 6,
		.p_data = read_rx,
		.data_length = sizeof(read_rx),
	},
	{	/* Erase */
		.dir = NRF_SQSPI_XFER_DIR_TX,
		.cmd = MX25_CMD_SECTOR_ERASE,
		.cmd_length = MX25_CMD_LEN,
		.address = CONFIG_SAMPLE_SQSPI_FLASH_ADDR,
		.addr_length = MX25_ADDRESS_LEN,
	},
	{	/* Page program */
		.dir = NRF_SQSPI_XFER_DIR_TX,
		.cmd = MX25_CMD_4PAGE_PROGRAM,
		.cmd_length = MX25_CMD_LEN,
		.address = CONFIG_SAMPLE_SQSPI_FLASH_ADDR,
		.addr_length = MX25_ADDRESS_LEN,
		.p_data = CONFIG_SAMPLE_SQSPI_MSG,
		.data_length = sizeof(CONFIG_SAMPLE_SQSPI_MSG)
	},
	{	/* Read status register */
		.dir = NRF_SQSPI_XFER_DIR_RX,
		.cmd = MX25_CMD_RDSR,
		.cmd_length = MX25_CMD_LEN,
		.p_data = &status_reg,
		.data_length = sizeof(status_reg),
	},
	{	/* Write enable */
		.dir = NRF_SQSPI_XFER_DIR_TX,
		.cmd = MX25_CMD_WREN,
		.cmd_length = MX25_CMD_LEN,
	},
	{	/* Write status register */
		.dir = NRF_SQSPI_XFER_DIR_TX,
		.cmd = MX25_CMD_WRSR,
		.cmd_length = MX25_CMD_LEN,
		.p_data = quad_enable,
		.data_length = sizeof(quad_enable),
	}
};

/* The sQSPI registers are in RAM shared with the FLPR core that runs the soft peripheral. */
static nrf_sqspi_t qspi = {
	.p_reg = (void *)DT_REG_ADDR(DT_NODELABEL(flpr_vri_ram)),
	.drv_inst_idx = 0
};

/* Set by the driver callback when a transfer is done. */
static volatile bool xfer_active;

ISR_DIRECT_DECLARE(sqspi_direct_isr)
{
	nrf_sqspi_irq_handler();
	return 0;
}

static int queue_pop(nrf_sqspi_xfer_t *xfer)
{
	unsigned int key = irq_lock();
	if(ring_buf_size_get(&evt_queue) < sizeof(*xfer))
	{
		irq_unlock(key);
		return -ENOMEM;
	}
	ring_buf_get(&evt_queue, (uint8_t *)xfer, sizeof(*xfer));
	irq_unlock(key);

	return 0;
}

static int queue_push(const nrf_sqspi_xfer_t *xfer)
{
	unsigned int key = irq_lock();
	if(ring_buf_space_get(&evt_queue) < sizeof(*xfer))
	{
		irq_unlock(key);
		return -ENOMEM;
	}
	ring_buf_put(&evt_queue, (uint8_t *)xfer, sizeof(*xfer));
	irq_unlock(key);

	return 0;

}

static nrf_sqspi_spi_lines_t get_lines(uint32_t cmd){
	switch (cmd)
	{
	case MX25_CMD_WREN:
	case MX25_CMD_SECTOR_ERASE:
	case MX25_CMD_RDSR:
	case MX25_CMD_WRSR:
		return NRF_SQSPI_SPI_LINES_SINGLE;
	default:
		return NRF_SQSPI_SPI_LINES_QUAD_1_4_4;
	}
}

static void xfer_done_cb(nrf_sqspi_t const *p_qspi, nrf_sqspi_evt_t *p_event, void *p_context);

static nrfx_err_t sqspi_xfer_index(void)
{
	nrfx_err_t err;
	uint8_t xfer_index;
	nrf_sqspi_xfer_t xfer;
	if(!queue_pop(&xfer)){
		const nrf_sqspi_dev_cfg_t dev_cfg = {
			.csn_pin = BOARD_EXTERNAL_MEMORY_SPIM_PIN_CSN,
			.sck_freq_khz = 1000,
			.spi_cpolpha = NRF_SQSPI_SPI_CPOLPHA_0,
			.mspi_lines = get_lines(xfer.cmd),
			.mspi_ddr = NRF_SQSPI_SPI_DDR_SINGLE,
		};

		err = nrf_sqspi_dev_cfg(&qspi, &dev_cfg, xfer_done_cb, NULL);
		if (err != NRFX_SUCCESS) {
			return err;
		}
		cmd = xfer.cmd;

		return nrf_sqspi_xfer(&qspi, &xfer, 1, 0);
	}
	return NRFX_ERROR_NO_MEM;
}

static void xfer_done_cb(nrf_sqspi_t const *p_qspi, nrf_sqspi_evt_t *p_event, void *p_context)
{
	if (p_event->type == NRF_SQSPI_EVT_XFER_DONE) {
		if (p_event->data.xfer_done != NRF_SQSPI_RESULT_OK) {
			LOG_ERR("xfer done error, err = %d", p_event->data.xfer_done);
		}
		if(cmd == MX25_CMD_4READ)
		{
			LOG_HEXDUMP_INF(read_rx, sizeof(read_rx), "Flash");
			if(!(status_reg & MX25_WEL_BIT)) {
				LOG_DBG("Write enable latch not set");
				// nrf_sqspi_xfer(&qspi, &xfers[RDSR_XFER_INDEX], 1, 0);
			}
		} else if(cmd == MX25_CMD_SECTOR_ERASE){
			if(!(status_reg & MX25_WEL_BIT)) {
				LOG_DBG("Write enable latch not set");
				// nrf_sqspi_xfer(&qspi, &xfers[RDSR_XFER_INDEX], 1, 0);
			}
		}
		if((status_reg & MX25_WIP_BIT)) {
			cmd = MX25_CMD_RDSR;
			const nrf_sqspi_dev_cfg_t dev_cfg = {
				.csn_pin = BOARD_EXTERNAL_MEMORY_SPIM_PIN_CSN,
				.sck_freq_khz = 1000,
				.spi_cpolpha = NRF_SQSPI_SPI_CPOLPHA_0,
				.mspi_lines = NRF_SQSPI_SPI_LINES_SINGLE,
				.mspi_ddr = NRF_SQSPI_SPI_DDR_SINGLE,
			};

			nrfx_err_t err = nrf_sqspi_dev_cfg(&qspi, &dev_cfg, xfer_done_cb, NULL);
			if (err != NRFX_SUCCESS) {
				LOG_ERR("dev cfg failed, err = %d", err);
			}

			nrf_sqspi_xfer_t rdsr_xfer = {	/* Read status register */
				.dir = NRF_SQSPI_XFER_DIR_RX,
				.cmd = MX25_CMD_RDSR,
				.cmd_length = MX25_CMD_LEN,
				.p_data = &status_reg,
				.data_length = sizeof(status_reg),
			};

			err = nrf_sqspi_xfer(&qspi, &rdsr_xfer, 1, 0);
			if(err != NRFX_SUCCESS) {
				LOG_ERR("Read status register failed, err = %d", err);
			}
		}
		else if(sqspi_xfer_index() != NRFX_SUCCESS){
			xfer_active = false;
		}
	}
}

static nrfx_err_t external_memory_init(void)
{
	nrfx_err_t err;
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
			}
		}
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
	nrf_sqspi_xfer_t rdsr_xfer = {	/* Read status register */
		.dir = NRF_SQSPI_XFER_DIR_RX,
		.cmd = MX25_CMD_RDSR,
		.cmd_length = MX25_CMD_LEN,
		.p_data = &status_reg,
		.data_length = sizeof(status_reg),
	};
	nrf_sqspi_xfer_t wren_xfer = {	/* Write enable */
		.dir = NRF_SQSPI_XFER_DIR_TX,
		.cmd = MX25_CMD_WREN,
		.cmd_length = MX25_CMD_LEN,
	};
	nrf_sqspi_xfer_t wrsr_xfer = {	/* Write status register */
		.dir = NRF_SQSPI_XFER_DIR_TX,
		.cmd = MX25_CMD_WRSR,
		.cmd_length = MX25_CMD_LEN,
		.p_data = quad_enable,
		.data_length = sizeof(quad_enable),
	};

	/* FLPR must be a secure bus master to access its RAM and MEMCONF. */
	nrf_spu_periph_perm_secattr_set(NRF_SPU00, nrf_address_slave_get((uint32_t)NRF_VPR), true);

	err = nrf_sqspi_init(&qspi, &cfg);
	if (err != NRFX_SUCCESS) {
		return err;
	}

	/* The driver uses standard drive, which is too weak for quad reads. */
	nrf_gpio_reconfigure(cfg.pins.sck, NULL, NULL, NULL, &drive, NULL);
	for (size_t i = 0; i < ARRAY_SIZE(cfg.pins.io); i++) {
		nrf_gpio_reconfigure(cfg.pins.io[i], NULL, NULL, NULL, &drive, NULL);
	}

	err = nrf_sqspi_dev_data_fmt_set(&qspi, &sqspi_data_fmt);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrf_sqspi_dev_data_fmt_set() failed: %d", err);
		return err;
	}

	BM_IRQ_DIRECT_CONNECT(SP_VPR_IRQn, 2, sqspi_direct_isr, 0);

	err = nrf_sqspi_activate(&qspi);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrf_sqspi_activate failed, err = %d", err);
		return err;
	}
	queue_push(&rdsr_xfer);
	queue_push(&wren_xfer);
	queue_push(&rdsr_xfer);
	queue_push(&wrsr_xfer);

	return NRFX_SUCCESS;
}

static nrfx_err_t flash_erase(void)
{
	int err;
	nrf_sqspi_xfer_t rdsr_xfer = {	/* Read status register */
		.dir = NRF_SQSPI_XFER_DIR_RX,
		.cmd = MX25_CMD_RDSR,
		.cmd_length = MX25_CMD_LEN,
		.p_data = &status_reg,
		.data_length = sizeof(status_reg),
	};
	nrf_sqspi_xfer_t wren_xfer = {	/* Write enable */
		.dir = NRF_SQSPI_XFER_DIR_TX,
		.cmd = MX25_CMD_WREN,
		.cmd_length = MX25_CMD_LEN,
	};
	nrf_sqspi_xfer_t erase_xfer = {	/* Erase */
		.dir = NRF_SQSPI_XFER_DIR_TX,
		.cmd = MX25_CMD_SECTOR_ERASE,
		.cmd_length = MX25_CMD_LEN,
		.address = CONFIG_SAMPLE_SQSPI_FLASH_ADDR,
		.addr_length = MX25_ADDRESS_LEN,
	};

	err = queue_push(&rdsr_xfer);
	if (err != 0){
		LOG_ERR("queue_push failed err = %d", err);
		return err;
	}

	err = queue_push(&wren_xfer);
	if (err != 0){
		LOG_ERR("queue_push failed err = %d", err);
		return err;
	}

	err = queue_push(&rdsr_xfer);
	if (err != 0){
		LOG_ERR("queue_push failed err = %d", err);
		return err;
	}

	err = queue_push(&erase_xfer);
	if (err != 0){
		LOG_ERR("queue_push failed err = %d", err);
		return err;
	} else {
		LOG_INF("Pushed erase to queue");
	}


	return NRFX_SUCCESS;
}

static nrfx_err_t flash_read(void)
{
	int err;
	nrf_sqspi_xfer_t rdsr_xfer = {	/* Read status register */
		.dir = NRF_SQSPI_XFER_DIR_RX,
		.cmd = MX25_CMD_RDSR,
		.cmd_length = MX25_CMD_LEN,
		.p_data = &status_reg,
		.data_length = sizeof(status_reg),
	};
	nrf_sqspi_xfer_t read_xfer = {	/* Read */
		.dir = NRF_SQSPI_XFER_DIR_RX,
		.cmd = MX25_CMD_4READ,
		.cmd_length = MX25_CMD_LEN,
		.address = CONFIG_SAMPLE_SQSPI_FLASH_ADDR,
		.addr_length = MX25_ADDRESS_LEN,
		.dummy_length = 6,
		.p_data = read_rx,
		.data_length = sizeof(read_rx),
	};

	err = queue_push(&rdsr_xfer);
	if (err != 0){
		LOG_ERR("queue_push failed err = %d", err);
		return err;
	}

	err = queue_push(&read_xfer);
	if (err != 0){
		LOG_ERR("queue_push failed err = %d", err);
		return err;
	} else {
		LOG_INF("Pushed read to queue");
	}

	return NRFX_SUCCESS;
}

static nrfx_err_t flash_program(void)
{
	int err;
	nrf_sqspi_xfer_t rdsr_xfer = {	/* Read status register */
		.dir = NRF_SQSPI_XFER_DIR_RX,
		.cmd = MX25_CMD_RDSR,
		.cmd_length = MX25_CMD_LEN,
		.p_data = &status_reg,
		.data_length = sizeof(status_reg),
	};
	nrf_sqspi_xfer_t wren_xfer = {	/* Write enable */
		.dir = NRF_SQSPI_XFER_DIR_TX,
		.cmd = MX25_CMD_WREN,
		.cmd_length = MX25_CMD_LEN,
	};
	nrf_sqspi_xfer_t p_prog_xfer = {	/* Page program */
		.dir = NRF_SQSPI_XFER_DIR_TX,
		.cmd = MX25_CMD_4PAGE_PROGRAM,
		.cmd_length = MX25_CMD_LEN,
		.address = CONFIG_SAMPLE_SQSPI_FLASH_ADDR,
		.addr_length = MX25_ADDRESS_LEN,
		.p_data = CONFIG_SAMPLE_SQSPI_MSG,
		.data_length = sizeof(CONFIG_SAMPLE_SQSPI_MSG)
	};

	err = queue_push(&rdsr_xfer);
	if (err != 0){
		LOG_ERR("queue_push failed err = %d", err);
		return err;
	}

	err = queue_push(&wren_xfer);
	if (err != 0){
		LOG_ERR("queue_push failed err = %d", err);
		return err;
	}

	err = queue_push(&rdsr_xfer);
	if (err != 0){
		LOG_ERR("queue_push failed err = %d", err);
		return err;
	}

	err = queue_push(&p_prog_xfer);
	if (err != 0){
		LOG_ERR("queue_push failed err = %d", err);
		return err;
	} else {
		LOG_INF("Pushed page program to queue");
	}

	return NRFX_SUCCESS;
}



static void button_handler(uint8_t pin, enum bm_buttons_evt_type action)
{
	nrfx_err_t err;

	if (action != BM_BUTTONS_PRESS) {
		return;
	}

	switch (pin) {
	case BOARD_PIN_BTN_1:
		err = flash_erase();
		if (err != NRFX_SUCCESS) {
			LOG_ERR("Flash erase failed, err = %d", err);
		}
		break;
	case BOARD_PIN_BTN_2:
		err = flash_read();
		if (err != NRFX_SUCCESS) {
			LOG_ERR("Flash read failed, err = %d", err);
		}
		break;
	case BOARD_PIN_BTN_3:
		err = flash_program();
		if (err != NRFX_SUCCESS) {
			LOG_ERR("Flash program failed, err = %d", err);
		}
		break;
	default:
		break;
	}
	if(xfer_active == false)
	{
		xfer_active = true;
		sqspi_xfer_index();
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
	nrfx_err_t nrfx_err;

	err = buttons_init();
	if (err) {
		LOG_ERR("Buttons init failed, err %d", err);
		goto idle;
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);

	nrfx_err = external_memory_init();
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("SPI init failed, err %d", err);
		goto idle;
	}

	LOG_INF("sQSPI sample initialized");
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
