/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <bm/bm_gpiote.h>
#include <nrfx_gpiote.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bm_gpiote, CONFIG_BM_GPIOTE_LOG_LEVEL);

struct bm_gpiote_inst {
	/* GPIOTE instance. */
	nrfx_gpiote_t instance;
	/* Mask of available ports for GPIOTE instance. */
	uint32_t available_gpio_ports;
};

#define BM_GPIOTE_INST_INITIALIZER(periph_name, prefix, idx, off_code)                             \
	[NRFX_CONCAT(NRFX_GPIOTE, idx, _INST_IDX)] = {                                             \
		.instance = {                                                                      \
			.p_reg = (NRF_GPIOTE_Type *)NRF_GPIOTE_INST_GET(idx),                      \
			.cb = { .drv_inst_idx = NRFX_CONCAT(NRFX_GPIOTE, idx, _INST_IDX) },        \
		},                                                                                 \
		.available_gpio_ports = NRFX_CONCAT_3(GPIOTE, idx, _AVAILABLE_GPIO_PORTS),         \
	},

#define BM_GPIOTE_ISR_DECLARE(periph_name, prefix, idx, off_code)                                  \
	ISR_DIRECT_DECLARE(NRFX_CONCAT_3(gpiote_, idx, _direct_isr))                               \
	{                                                                                          \
		nrfx_gpiote_irq_handler(                                                           \
			&gpiote_instances[NRFX_CONCAT(NRFX_GPIOTE, idx, _INST_IDX)].instance);     \
		return 0;                                                                          \
	}

#define BM_GPIOTE_IRQ_CONNECT(periph_name, prefix, idx, off_code)                                  \
	IRQ_DIRECT_CONNECT(                                                                        \
		NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(idx)) + NRF_GPIOTE_IRQ_GROUP,              \
		CONFIG_BM_GPIOTE_IRQ_PRIO, NRFX_CONCAT_3(gpiote_, idx, _direct_isr), 0)

static struct bm_gpiote_inst gpiote_instances[GPIOTE_COUNT] = {
	NRFX_FOREACH_INDEXED_PRESENT(GPIOTE, BM_GPIOTE_INST_INITIALIZER, (), ())
};

NRFX_FOREACH_INDEXED_PRESENT(GPIOTE, BM_GPIOTE_ISR_DECLARE, (), ())

nrfx_gpiote_t *bm_gpiote_instance_get(uint8_t port)
{
	for (int i = 0; i < ARRAY_SIZE(gpiote_instances); i++) {
		if (gpiote_instances[i].available_gpio_ports & NRFX_BIT(port)) {
			return &gpiote_instances[i].instance;
		}
	}

	return NULL;
}

static int bm_gpiote_init(void)
{
	int err;

	for (int i = 0; i < ARRAY_SIZE(gpiote_instances); i++) {
		if (!nrfx_gpiote_init_check(&gpiote_instances[i].instance)) {
			err = nrfx_gpiote_init(&gpiote_instances[i].instance, 0);
			if (err) {
				LOG_ERR("Failed to initialize gpiote, err %d", err);
				return err;
			}
		}
	}

	NRFX_FOREACH_INDEXED_PRESENT(GPIOTE, BM_GPIOTE_IRQ_CONNECT, (;), ())

	return 0;
}

SYS_INIT(bm_gpiote_init, APPLICATION, 0);
