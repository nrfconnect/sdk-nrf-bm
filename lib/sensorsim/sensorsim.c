/*
 * Copyright (c) 2012 - 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <bm/lib/sensorsim.h>

int sensorsim_init(struct sensorsim_state *state, const struct sensorsim_cfg *cfg)
{
	if (!state || !cfg) {
		return -EFAULT;
	}

	if (cfg->max < cfg->min) {
		return -EINVAL;
	}

	if (cfg->start_at_max) {
		state->val = cfg->max;
		state->is_increasing = false;
	} else {
		state->val = cfg->min;
		state->is_increasing = true;
	}

	state->cfg = *cfg;
	return 0;
}

int sensorsim_measure(struct sensorsim_state *state, uint32_t *value)
{
	if (!state || !value) {
		return -EFAULT;
	}

	struct sensorsim_cfg *const cfg = &state->cfg;

	if (state->is_increasing) {
		if (cfg->max - state->val > cfg->incr) {
			state->val += cfg->incr;
		} else {
			state->val = cfg->max;
			state->is_increasing = false;
		}
	} else {
		if (state->val - cfg->min > cfg->incr) {
			state->val -= cfg->incr;
		} else {
			state->val = cfg->min;
			state->is_increasing = true;
		}
	}

	*value = state->val;
	return 0;
}
