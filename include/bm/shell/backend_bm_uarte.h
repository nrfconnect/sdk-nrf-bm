/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* Get pointer to BM UARTE shell backend */
const struct shell *shell_backend_bm_uarte_get_ptr(void);

/* Check if RX data is ready to process */
bool shell_backend_bm_uarte_rx_ready(void);
