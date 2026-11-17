/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* The platform on sdk-nrf-bm. The SoftDevice handler library forwards the stack's
 * interrupts, runs its reset handler at system init, and hands SoC events to the
 * observer here from the stack's event interrupt. NVM writes go through
 * sd_flash_write(), which owns RRAMC while the stack is enabled.
 */

#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/sys/reboot.h>

#include <psa/crypto.h>

#include <bm/softdevice_handler/nrf_sdh_soc.h>

#include <nrf_error.h>
#include <nrf_soc.h>

#include <nrfx.h>

#define RAND_SEED_LEN 32U
#define WORD_LEN      4U
/* The bytes one sd_flash_write() call writes at most. */
#define STAGE_LEN     256U

_Static_assert((STAGE_LEN % WORD_LEN) == 0U, "the stage holds whole words");
_Static_assert((PLATFORM_SLOT_ADDR % WORD_LEN) == 0U, "the update slot is word aligned");

enum flash_state {
	FLASH_IDLE,
	FLASH_BUSY,
	FLASH_FAILED,
};

static volatile enum flash_state flash_state;

/* sd_flash_write() reads the source until the write completes. */
static uint32_t stage[STAGE_LEN / WORD_LEN];

void platform_halt(void)
{
	for (;;) {
		__WFE();
	}
}

void platform_reset(void)
{
	sys_reboot(SYS_REBOOT_COLD);
	platform_halt();
}

void platform_secret_clear(uint8_t *buf, size_t len)
{
	volatile uint8_t *const p = buf;

	for (size_t i = 0U; i < len; i++) {
		p[i] = 0U;
	}
}

bool platform_stack_seed(void)
{
	uint8_t seed[RAND_SEED_LEN];
	const bool ok = (psa_generate_random(seed, sizeof(seed)) == PSA_SUCCESS) &&
			(sd_rand_seed_set(seed) == NRF_SUCCESS);

	platform_secret_clear(seed, sizeof(seed));
	return ok;
}

/* Serves one SoC event, in the stack's event interrupt. A seed request that cannot be
 * served stops the device, as a failed bring-up does. */
static void soc_event_serve(uint32_t evt, void *context)
{
	(void)context;

	switch (evt) {
	case NRF_EVT_RAND_SEED_REQUEST:
		if (!platform_stack_seed()) {
			platform_halt();
		}
		break;
	case NRF_EVT_FLASH_OPERATION_SUCCESS:
		flash_state = FLASH_IDLE;
		break;
	case NRF_EVT_FLASH_OPERATION_ERROR:
		flash_state = FLASH_FAILED;
		break;
	default:
		/* An event that needs no reply. */
		break;
	}
}

NRF_SDH_SOC_OBSERVER(soc_event_observer, soc_event_serve, NULL, USER);

/* Writes the whole words at @p word_addr from the stage, and waits for the stack to
 * report the write done. */
static bool stage_write(uint32_t word_addr, uint32_t word_count)
{
	flash_state = FLASH_BUSY;
	if (sd_flash_write((uint32_t *)(uintptr_t)word_addr, stage, word_count) != NRF_SUCCESS) {
		flash_state = FLASH_IDLE;
		return false;
	}
	/* The event interrupt ends the wait: its return sets the event register. */
	while (flash_state == FLASH_BUSY) {
		__WFE();
	}
	return flash_state == FLASH_IDLE;
}

bool platform_nvm_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
	if ((addr < PLATFORM_SLOT_ADDR) || (len > PLATFORM_SLOT_SIZE) ||
	    ((addr - PLATFORM_SLOT_ADDR) > (PLATFORM_SLOT_SIZE - len))) {
		return false;
	}

	uint32_t word_addr = addr - (addr % WORD_LEN);
	uint32_t head = addr - word_addr;

	while (len > 0U) {
		uint32_t take = STAGE_LEN - head;

		if (take > len) {
			take = len;
		}

		const uint32_t span = head + take;
		const uint32_t word_count = (span + WORD_LEN - 1U) / WORD_LEN;

		/* RRAM needs no erase: the bytes around the range are written back as
		 * they are. */
		memcpy(stage, (const void *)(uintptr_t)word_addr, word_count * WORD_LEN);
		memcpy((uint8_t *)stage + head, data, take);
		if (!stage_write(word_addr, word_count)) {
			return false;
		}

		word_addr += word_count * WORD_LEN;
		data += take;
		len -= take;
		head = 0U;
	}
	return true;
}
