/*
 * Copyright Runtime.io 2018. All rights reserved.
 * Copyright (c) 2021-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/net_buf.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <zephyr/mgmt/mcumgr/transport/smp.h>

#include <mgmt/mcumgr/transport/smp_reassembly.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mcumgr_smp, CONFIG_MCUMGR_TRANSPORT_LOG_LEVEL);

/* To be able to unit test some callers some functions need to be
 * demoted to allow overriding them.
 */
#ifdef CONFIG_ZTEST
#define WEAK __weak
#else
#define WEAK
#endif

NET_BUF_POOL_DEFINE(pkt_pool, CONFIG_MCUMGR_TRANSPORT_NETBUF_COUNT,
		    CONFIG_MCUMGR_TRANSPORT_NETBUF_SIZE, 0, NULL);

struct net_buf *smp_packet_alloc(void)
{
	return net_buf_alloc(&pkt_pool, K_NO_WAIT);
}

void smp_packet_free(struct net_buf *nb)
{
	net_buf_unref(nb);
}

/**
 * @brief Allocates a response buffer.
 *
 * If a source buf is provided, its user data is copied into the new buffer.
 *
 * @param req		An optional source buffer to copy user data from.
 * @param arg		The streamer providing the callback.
 *
 * @return	Newly-allocated buffer on success
 *		NULL on failure.
 */
void *smp_alloc_rsp(const void *req, void *arg)
{
	const struct net_buf *req_nb;
	struct net_buf *rsp_nb;

	req_nb = req;
	rsp_nb = smp_packet_alloc();

	if (rsp_nb == NULL) {
		return NULL;
	}

	return rsp_nb;
}

void smp_free_buf(void *buf, void *arg)
{
	if (!buf) {
		return;
	}

	smp_packet_free(buf);
}

int smp_transport_init(struct smp_transport *smpt)
{
	__ASSERT((smpt->functions.output != NULL),
		 "Required transport output function pointer cannot be NULL");

	if (smpt->functions.output == NULL) {
		return -EINVAL;
	}

#ifdef CONFIG_MCUMGR_TRANSPORT_REASSEMBLY
	smp_reassembly_init(smpt);
#endif

	return 0;
}

/**
 * @brief Enqueues an incoming SMP request packet for processing.
 *
 * This function always consumes the supplied net_buf.
 *
 * @param smpt                  The transport to use to send the corresponding
 *                                  response(s).
 * @param nb                    The request packet to process.
 */
WEAK void
smp_rx_req(struct smp_transport *smpt, struct net_buf *nb)
{
	struct cbor_nb_reader reader;
	struct cbor_nb_writer writer;
	struct smp_streamer streamer;

	streamer = (struct smp_streamer) {
		.reader = &reader,
		.writer = &writer,
		.smpt = smpt,
	};

	(void)smp_process_request_packet(&streamer, nb);
}
