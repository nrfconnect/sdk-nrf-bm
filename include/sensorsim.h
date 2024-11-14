/**
 * Copyright (c) 2012 - 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup sensorsim Sensor data simulator library
 * @{
 * @brief Functions for simulating sensor data.
 *
 * Currently only a triangular waveform simulator is implemented.
 */

#ifndef SENSORSIM_H__
#define SENSORSIM_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Triangular waveform sensor simulator configuration.
 */
struct sensorsim_cfg {
	/**
	 * @brief Minimum simulated value.
	 */
	uint32_t min;
	/**
	 * @brief Maximum simulated value.
	 */
	uint32_t max;
	/**
	 * @brief Increment between each measurement.
	 */
	uint32_t incr;
	/**
	 * @brief If measurement should start at the maximum value instead of the minimum value.
	 */
	bool start_at_max;
};

/**
 * @brief Triangular waveform sensor simulator state.
 *
 * Used internally by the module.
 */
struct sensorsim_state {
	/**
	 * @brief Current sensor value.
	 */
	uint32_t val;
	/**
	 * @brief Sensor simulator configuration. Populated when calling @p sensorsim_init.
	 */
	struct sensorsim_cfg cfg;
	/**
	 * @brief If the simulator is in increasing state.
	 */
	bool is_increasing;
};

/**
 * @brief Function for initializing a simple triangular waveform sensor simulator.
 *
 * @param[out]  state  Current state of simulator.
 * @param[in]   cfg    Simulator configuration. It is safe to deallocate or let this go
 *                     out of scope after returning from the function.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p state or @p cfg is @c NULL.
 * @retval -EINVAL If minimum simulated value is greater than maximum simulated value in @p cfg.
 */
int sensorsim_init(struct sensorsim_state *state, const struct sensorsim_cfg *cfg);

/**
 * @brief Function for generating a simulated sensor measurement using a triangular wave generator.
 *
 * @param[in,out]  state  Current state of simulator.
 * @param[out]     value  Simulator output value.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p state or @p value is @c NULL.
 */
int sensorsim_measure(struct sensorsim_state *state, uint32_t *value);

#ifdef __cplusplus
}
#endif

#endif /* SENSORSIM_H__ */

/** @} */
