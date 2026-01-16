/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/atomic.h>
#include <nfc/tnep/tag_signalling.h>
#include <bm/nfc/tnep/tag.h>

static atomic_t msg_rx_event;
static atomic_t msg_tx_event;

static void nfc_tnep_tag_signalling_init(void)
{
	(void)atomic_set(&msg_rx_event, TNEP_EVENT_DUMMY);
	(void)atomic_set(&msg_tx_event, TNEP_EVENT_DUMMY);
}

int nfc_tnep_tag_init(nfc_payload_set_t payload_set)
{
	nfc_tnep_tag_signalling_init();

	return nfc_tnep_tag_internal_init(payload_set);
}

void nfc_tnep_tag_signalling_rx_event_raise(enum tnep_event event)
{
	(void)atomic_set(&msg_rx_event, event);
}

void nfc_tnep_tag_signalling_tx_event_raise(enum tnep_event event)
{
	(void)atomic_set(&msg_tx_event, event);
}

static bool event_check_and_clear(atomic_t *msg_event, enum tnep_event *event)
{
	enum tnep_event e = atomic_get(msg_event);

	if (e != TNEP_EVENT_DUMMY) {
		(void)atomic_cas(msg_event, e, TNEP_EVENT_DUMMY);
		*event = e;
		return true;
	}

	return false;
}

bool nfc_tnep_tag_signalling_rx_event_check_and_clear(enum tnep_event *event)
{
	return event_check_and_clear(&msg_rx_event, event);
}

bool nfc_tnep_tag_signalling_tx_event_check_and_clear(enum tnep_event *event)
{
	return event_check_and_clear(&msg_tx_event, event);
}
