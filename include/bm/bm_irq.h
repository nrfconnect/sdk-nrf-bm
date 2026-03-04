/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup bm_irq NCS Bare Metal IRQ library
 * @{
 */

#ifndef BM_IRQ_H__
#define BM_IRQ_H__

#include <zephyr/irq.h>
#include <zephyr/sys/util_macro.h>

/**
 * @brief Priority offset between Bare Metal and Zephyr interrupt priorities.
 *
 * Zephyr offsets interrupt priorities by one for each zero latency interrupt level used
 * and one more if programmable faults are enabled.
 * This value is used to remap Bare Metal NVIC priorities to the corresponding Zephyr priorities.
 */
#if defined(_IRQ_PRIO_OFFSET)
#define BM_IRQ_OFFSET _IRQ_PRIO_OFFSET
#else
#define BM_IRQ_OFFSET 0
#endif

/**
 * @brief Does a bitwise AND operation on _flags to check if IRQ_ZERO_LATENCY is set.
 */
#define IS_ZERO_LATENCY(_flags)									   \
	COND_CODE_1(CONFIG_ZERO_LATENCY_IRQS, ((_flags) & IRQ_ZERO_LATENCY), (false))

/**
 * @brief Connect an IRQ to a handler with the specified priority.
 *
 * Macro that uses the Zephyr @c IRQ_DIRECT_CONNECT macro but remaps the priority
 * to match the NVIC range. The specified @p _prio value corresponds directly
 * to the NVIC priority.
 *
 * A build-time assertion verifies that @p _prio is within the valid range.
 *
 * @param _irqn   IRQ number.
 * @param _prio   NVIC interrupt priority.
 * @param _handler Interrupt handler function.
 * @param _flags  Flags passed to @c IRQ_DIRECT_CONNECT.
 */
#define BM_IRQ_DIRECT_CONNECT(_irqn, _prio, _handler, _flags)					   \
	IF_ENABLED(CONFIG_ZERO_LATENCY_IRQS,							   \
		   (BUILD_ASSERT(!IS_ZERO_LATENCY(_flags) || ((ZERO_LATENCY_LEVELS == 1) ||	   \
				 ((_prio) < ZERO_LATENCY_LEVELS)),				   \
				 "IRQ priority of " STRINGIFY(_handler)				   \
				 " is >= " STRINGIFY(ZERO_LATENCY_LEVELS) " and zero latency")));  \
	BUILD_ASSERT(IS_ZERO_LATENCY(_flags) || ((_prio) >= BM_IRQ_OFFSET),			   \
		     "IRQ priority of " STRINGIFY(_handler) " is < " STRINGIFY(BM_IRQ_OFFSET));	   \
	IRQ_DIRECT_CONNECT(_irqn, (IS_ZERO_LATENCY(_flags) ? (_prio) : (_prio - BM_IRQ_OFFSET)),   \
			   _handler, (_flags))

/**
 * @brief Sets the priority of an IRQ.
 *
 * Wrapper around @c NVIC_SetPriority that verifies at build time that the
 * priority is within the valid range (2–7).
 *
 * * A build-time assertion verifies that @p _prio is within the valid range.
 *
 * @param _irqn   IRQ number.
 * @param _prio   NVIC interrupt priority.
 */
#define BM_IRQ_SET_PRIORITY(_irqn, _prio)                                                         \
	BUILD_ASSERT(_prio >= BM_IRQ_OFFSET,                                                      \
		     "IRQ priority of " STRINGIFY(_irqn) " is < " STRINGIFY(BM_IRQ_OFFSET));      \
	NVIC_SetPriority(_irqn, _prio)

#endif /* BM_IRQ_H__ */

/** @} */
