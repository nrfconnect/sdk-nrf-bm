/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int IRQn_Type;

void NVIC_ClearPendingIRQ(IRQn_Type IRQn);
void NVIC_EnableIRQ(IRQn_Type IRQn);

#ifdef __cplusplus
}
#endif
