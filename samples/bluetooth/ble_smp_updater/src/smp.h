/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* The SMP server: one request packet in, one response packet out, with no transport
 * framing.
 */

#ifndef BLE_SMP_SMP_H_
#define BLE_SMP_SMP_H_

#include <stdbool.h>
#include <stdint.h>

/* The largest SMP request packet, header included, that the transport reassembles. */
#define SMP_REQ_MAX 1280U

/*
 * Serves the SMP packet @p pkt of @p len bytes, header included. Writes the response
 * packet into @p rsp and returns its length, or 0 when the request gets no response.
 * Sets @p is_reset when the request was the reset command, which the caller carries
 * out once the response has gone out.
 */
uint32_t smp_packet_serve(const uint8_t *pkt, uint32_t len, uint8_t *rsp, uint32_t rsp_size,
			  bool *is_reset);

#endif /* BLE_SMP_SMP_H_ */
