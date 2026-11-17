/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* The sample's runtime platform: NVM writes through the BLE stack, its SoC events,
 * the update slot, and reset.
 */

#ifndef BLE_SMP_PLATFORM_H_
#define BLE_SMP_PLATFORM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/devicetree.h>

/* The NVM the update writes, its start address and size in bytes. */
#define PLATFORM_SLOT_ADDR ((uint32_t)DT_REG_ADDR(DT_NODELABEL(update_partition)))
#define PLATFORM_SLOT_SIZE ((uint32_t)DT_REG_SIZE(DT_NODELABEL(update_partition)))

/* Stops. */
__attribute__((noreturn)) void platform_halt(void);

__attribute__((noreturn)) void platform_reset(void);

/* Clears sensitive data with stores the compiler keeps. */
void platform_secret_clear(uint8_t *buf, size_t len);

/* Gives the BLE stack a fresh random seed. The stack's later seed requests are
 * served from its event interrupt. */
bool platform_stack_seed(void);

/*
 * Writes @p len bytes to NVM at @p addr, which lies in the update slot, and returns
 * once the BLE stack has written them. Bytes before and after the range that share
 * its first and last word keep their value.
 */
bool platform_nvm_write(uint32_t addr, const uint8_t *data, uint32_t len);

#endif /* BLE_SMP_PLATFORM_H_ */
