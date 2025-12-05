/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>  /* IS_ENABLED, ISR_DIRECT_DECLARE, IRQ_DIRECT_CONNECT */

#if defined(CONFIG_SOFTDEVICE)
#include <nrf_soc.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#endif

#include <zephyr/sys/__assert.h>

#include <nrfx_nfct.h>
#include <nrfx_timer.h>
#include <hal/nrf_ficr.h>

#include <nfc_platform.h>

#include "platform_internal.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(nfc_platform, CONFIG_NFC_PLATFORM_LOG_LEVEL);

#if NRF54L_ERRATA_60_ENABLE_WORKAROUND
#define NFC_PLATFORM_USE_TIMER_WORKAROUND 1
#else
#define NFC_PLATFORM_USE_TIMER_WORKAROUND 0
#endif

#if NFC_PLATFORM_USE_TIMER_WORKAROUND
#define NFC_TIMER_IRQn NRFX_CONCAT_3(TIMER, NRFX_NFCT_CONFIG_TIMER_INSTANCE_ID, _IRQn)
#endif /* NFC_PLATFORM_USE_TIMER_WORKAROUND */

#define NFC_T2T_BUFFER_SIZE (IS_ENABLED(CONFIG_NFC_T2T_NRFXLIB) ? NFC_PLATFORM_T2T_BUFFER_SIZE : 0U)
#define NFC_T4T_BUFFER_SIZE (IS_ENABLED(CONFIG_NFC_T4T_NRFXLIB) ? \
							    (2 * NFC_PLATFORM_T4T_BUFFER_SIZE) : 0U)

#define NFCT_PLATFORM_BUFFER_SIZE MAX(NFC_T4T_BUFFER_SIZE, NFC_T2T_BUFFER_SIZE)

/* NFC platform buffer. This buffer is used directly by the NFCT peripheral.
 * It might need to be allocated in the specific memory section which can be accessed
 * by EasyDMA.
 */
static uint8_t nfc_platform_buffer[NFCT_PLATFORM_BUFFER_SIZE];

#if CONFIG_NFC_T2T_NRFXLIB
	BUILD_ASSERT(sizeof(nfc_platform_buffer) >= NFC_T2T_BUFFER_SIZE,
		     "Minimal buffer size for the NFC T2T operations must be at least 16 bytes");
#endif /* CONFIG_NFC_T2T_NRFXLIB */

#if CONFIG_NFC_T4T_NRFXLIB
	BUILD_ASSERT(sizeof(nfc_platform_buffer) >= NFC_T4T_BUFFER_SIZE,
		     "Minimal buffer size for the NFC T4T operations must be at least 518 bytes");
#endif /* CONFIG_NFC_T4T_NRFXLIB */

ISR_DIRECT_DECLARE(nfc_isr_wrapper)
{
	nrfx_nfct_irq_handler();

	ISR_DIRECT_PM();
	return 0;
}

#if defined(CONFIG_SOFTDEVICE)

static void on_soc_evt(uint32_t evt, void *ctx)
{
	if (evt == NRF_EVT_HFCLKSTARTED) {
		LOG_DBG("HFCLK clock started, activating NFC");
		nrfx_nfct_state_force(NRFX_NFCT_STATE_ACTIVATED);
	}
}
NRF_SDH_SOC_OBSERVER(nfc_sdh_soc, on_soc_evt, NULL, USER_LOW);

#endif /* defined(CONFIG_SOFTDEVICE) */

int nfc_platform_setup(nfc_lib_cb_resolve_t nfc_lib_cb_resolve, uint8_t *p_irq_priority)
{
	int err;

	IRQ_DIRECT_CONNECT(NFCT_IRQn, CONFIG_NFCT_IRQ_PRIORITY,
			   nfc_isr_wrapper, 0);

#if NFC_PLATFORM_USE_TIMER_WORKAROUND
	IRQ_DIRECT_CONNECT(NFC_TIMER_IRQn, CONFIG_NFCT_IRQ_PRIORITY,
			   nrfx_nfct_workaround_timer_handler, 0);
#endif /* NFC_PLATFORM_USE_TIMER_WORKAROUND */

	*p_irq_priority = CONFIG_NFCT_IRQ_PRIORITY;

	err = nfc_platform_internal_init(nfc_lib_cb_resolve);
	if (err) {
		LOG_ERR("NFC platform init fail: callback resolution function pointer is invalid");
		return -EFAULT;
	}

	LOG_DBG("NFC platform initialized");
	return 0;
}

static int nfc_platform_tagheaders_get(uint32_t tag_header[3])
{
	tag_header[0] = nrf_ficr_nfc_tagheader_get(NRF_FICR, 0);
	tag_header[1] = nrf_ficr_nfc_tagheader_get(NRF_FICR, 1);
	tag_header[2] = nrf_ficr_nfc_tagheader_get(NRF_FICR, 2);

	return 0;
}

int nfc_platform_nfcid1_default_bytes_get(uint8_t * const buf,
					  uint32_t        buf_len)
{
	if (!buf) {
		return -EINVAL;
	}

	if ((buf_len != NRFX_NFCT_NFCID1_SINGLE_SIZE) &&
	    (buf_len != NRFX_NFCT_NFCID1_DOUBLE_SIZE) &&
	    (buf_len != NRFX_NFCT_NFCID1_TRIPLE_SIZE)) {
		return -E2BIG;
	}

	int err;
	uint32_t nfc_tag_header[3];

	err = nfc_platform_tagheaders_get(nfc_tag_header);
	if (err != 0) {
		return err;
	}

	buf[0] = (uint8_t) (nfc_tag_header[0] >> 0);
	buf[1] = (uint8_t) (nfc_tag_header[0] >> 8);
	buf[2] = (uint8_t) (nfc_tag_header[0] >> 16);
	buf[3] = (uint8_t) (nfc_tag_header[1] >> 0);

	if (buf_len != NRFX_NFCT_NFCID1_SINGLE_SIZE) {
		buf[4] = (uint8_t) (nfc_tag_header[1] >> 8);
		buf[5] = (uint8_t) (nfc_tag_header[1] >> 16);
		buf[6] = (uint8_t) (nfc_tag_header[1] >> 24);

		if (buf_len == NRFX_NFCT_NFCID1_TRIPLE_SIZE) {
			buf[7] = (uint8_t) (nfc_tag_header[2] >> 0);
			buf[8] = (uint8_t) (nfc_tag_header[2] >> 8);
			buf[9] = (uint8_t) (nfc_tag_header[2] >> 16);
		}
		/* Workaround for errata 181 "NFCT: Invalid value in FICR for double-size NFCID1"
		 * found at the Errata document for your device located at
		 * https://infocenter.nordicsemi.com/index.jsp
		 */
		else if (buf[3] == 0x88) {
			buf[3] |= 0x11;
		}
	}

	return 0;
}

uint8_t *nfc_platform_buffer_alloc(size_t size)
{
	if (size > sizeof(nfc_platform_buffer)) {
		__ASSERT_NO_MSG(false);
		return NULL;
	}

	return nfc_platform_buffer;
}

void nfc_platform_buffer_free(uint8_t *p_buffer)
{
	ARG_UNUSED(p_buffer);
}

void nfc_platform_event_handler(nrfx_nfct_evt_t const *event)
{
#if defined(CONFIG_SOFTDEVICE)
	uint32_t sd_res;
#endif

	switch (event->evt_id) {
	case NRFX_NFCT_EVT_FIELD_DETECTED:
		LOG_DBG("Field detected");

#if defined(CONFIG_SOFTDEVICE)
		/* We need to start HFCLK through SoftDevice API. As the code
		 * executes from an IRQ it must be ensured that the IRQ priority
		 * is acceptable to call SoftDevice API. See CONFIG_NFCT_IRQ_PRIORITY.
		 */
		sd_res = sd_clock_hfclk_request();
		__ASSERT_NO_MSG(sd_res == NRF_SUCCESS);
#else
#error "No supported clock control"
#endif
		break;
	case NRFX_NFCT_EVT_FIELD_LOST:
		LOG_DBG("Field lost");

#if defined(CONFIG_SOFTDEVICE)
		sd_res = sd_clock_hfclk_release();
		__ASSERT_NO_MSG(sd_res == NRF_SUCCESS);
#else
#error "No supported clock control"
#endif
		break;
	default:
		/* No implementation required */
		break;
	}
}
