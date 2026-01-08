/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <bm/bm_buttons.h>
#include <hal/nrf_gpio.h>
#include <board-config.h>
#if defined(CONFIG_SOFTDEVICE)
#include <bm/softdevice_handler/nrf_sdh.h>
#endif

#include <nfc_t4t_lib.h>
#include <nfc/tnep/tag.h>
#include <bm/nfc/tnep/tag_signalling_bm.h>
#include <nfc/t4t/ndef_file.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>

LOG_MODULE_REGISTER(app, CONFIG_APP_NFC_TNEP_TAG_LOG_LEVEL);

#define TNEP_INITIAL_MSG_RECORD_COUNT 1

#define NDEF_TNEP_MSG_SIZE	1024

#define LED_INIT		BOARD_PIN_LED_0
#define LED_SVC_ONE		BOARD_PIN_LED_2
#define LED_SVC_TWO		BOARD_PIN_LED_3

#define BUTTON1_PIN		BOARD_PIN_BTN_0

static const uint8_t en_code[] = {'e', 'n'};

static const uint8_t svc_one_uri[] = "svc:pi";
static const uint8_t svc_two_uri[] = "svc:e";

static uint8_t tag_buffer[NDEF_TNEP_MSG_SIZE];
static uint8_t tag_buffer2[NDEF_TNEP_MSG_SIZE];

static void leds_init(void)
{
	nrf_gpio_cfg_output(LED_INIT);
	nrf_gpio_cfg_output(LED_SVC_ONE);
	nrf_gpio_cfg_output(LED_SVC_TWO);
}

static void led_init_on(void)
{
	nrf_gpio_pin_write(LED_INIT, BOARD_LED_ACTIVE_STATE);
}

static void led_svc_one_on(void)
{
	nrf_gpio_pin_write(LED_SVC_ONE, BOARD_LED_ACTIVE_STATE);
}

static void led_svc_one_off(void)
{
	nrf_gpio_pin_write(LED_SVC_ONE, !BOARD_LED_ACTIVE_STATE);
}

static void led_svc_two_on(void)
{
	nrf_gpio_pin_write(LED_SVC_TWO, BOARD_LED_ACTIVE_STATE);
}

static void led_svc_two_off(void)
{
	nrf_gpio_pin_write(LED_SVC_TWO, !BOARD_LED_ACTIVE_STATE);
}

static void tnep_svc_one_selected(void)
{
	int err;
	static const char svc_one_msg[] = "Service pi = 3.14159265358979323846";

	printk("Service one selected\n");

	NFC_TNEP_TAG_APP_MSG_DEF(app_msg, 1);
	NFC_NDEF_TEXT_RECORD_DESC_DEF(svc_one_rec, UTF_8, en_code,
				      sizeof(en_code), svc_one_msg,
				      strlen(svc_one_msg));

	err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(app_msg),
				      &NFC_NDEF_TEXT_RECORD_DESC(svc_one_rec));

	err = nfc_tnep_tag_tx_msg_app_data(&NFC_NDEF_MSG(app_msg),
					   NFC_TNEP_STATUS_SUCCESS);
	if (err) {
		printk("Service app data set err: %d\n", err);
	}

	led_svc_one_on();
}

static void tnep_svc_one_deselected(void)
{
	printk("Service one deselected\n");

	led_svc_one_off();
}

static void tnep_svc_one_msg_received(const uint8_t *data, size_t len)
{
	int err;

	printk("New service data received. Finish service data exchange\n");

	err = nfc_tnep_tag_tx_msg_no_app_data();
	if (err) {
		printk("TNEP Service data finish err: %d\n", err);
	}
}

static void tnep_svc_error(int err_code)
{
	printk("Service data exchange error: %d\n", err_code);
}

static void tnep_svc_two_selected(void)
{
	printk("Service two selected\n");

	led_svc_two_on();
}

static void tnep_svc_two_deselected(void)
{
	printk("Service two deselected\n");

	led_svc_two_off();
}

static void tnep_svc_two_msg_received(const uint8_t *data, size_t len)
{
	int err;

	printk("New service data received. Finish service data exchange\n");

	err = nfc_tnep_tag_tx_msg_no_app_data();
	if (err) {
		printk("TNEP Service data finish err: %d\n", err);
	}
}

NFC_TNEP_TAG_SERVICE_DEF(svc_one, svc_one_uri, (ARRAY_SIZE(svc_one_uri) - 1),
		     NFC_TNEP_COMM_MODE_SINGLE_RESPONSE, 20, 4, 1024,
		     tnep_svc_one_selected, tnep_svc_one_deselected,
		     tnep_svc_one_msg_received, tnep_svc_error);

NFC_TNEP_TAG_SERVICE_DEF(svc_two, svc_two_uri, (ARRAY_SIZE(svc_two_uri) - 1),
		     NFC_TNEP_COMM_MODE_SINGLE_RESPONSE, 63, 4, 1024,
		     tnep_svc_two_selected, tnep_svc_two_deselected,
		     tnep_svc_two_msg_received, tnep_svc_error);

static void nfc_callback(void *context, nfc_t4t_event_t event,
			 const uint8_t *data, size_t data_length, uint32_t flags)
{
	switch (event) {
	case NFC_T4T_EVENT_NDEF_UPDATED:
		if (data_length > 0) {
			nfc_tnep_tag_rx_msg_indicate(nfc_t4t_ndef_file_msg_get(data),
						     data_length);
		}

		break;

	case NFC_T4T_EVENT_FIELD_ON:
	case NFC_T4T_EVENT_FIELD_OFF:
		nfc_tnep_tag_on_selected();

		break;

	default:
		/* This block intentionally left blank. */
		break;
	}
}

static int tnep_initial_msg_encode(struct nfc_ndef_msg_desc *msg)
{
	const uint8_t text[] = "Hello World";

	NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_text, UTF_8, en_code,
				      sizeof(en_code), text,
				      strlen(text));

	return nfc_tnep_initial_msg_encode(msg,
					   &NFC_NDEF_TEXT_RECORD_DESC(nfc_text),
					   1);
}

static void on_button1_press(void)
{
	int err;
	static const char svc_two_msg[] = "Service e  = 2.71828182845904523536";

	NFC_TNEP_TAG_APP_MSG_DEF(app_msg, 1);
	NFC_NDEF_TEXT_RECORD_DESC_DEF(svc_two_rec, UTF_8, en_code,
					sizeof(en_code), svc_two_msg,
					strlen(svc_two_msg));

	err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(app_msg),
					&NFC_NDEF_TEXT_RECORD_DESC(svc_two_rec));

	err = nfc_tnep_tag_tx_msg_app_data(&NFC_NDEF_MSG(app_msg),
						NFC_TNEP_STATUS_SUCCESS);
	if (err == -EACCES) {
		printk("Service is not in selected state. App data cannot be set\n");
	} else {
		printk("Service app data set err: %d\n", err);
	}
}

static void bm_button_handler(uint8_t pin, uint8_t action)
{
	if (action == BM_BUTTONS_PRESS) {
		switch (pin) {
		case BUTTON1_PIN:
			on_button1_press();
			break;

		default:
			break;
		}
	}
}

static int board_buttons_init(void)
{
	int err;

	static const struct bm_buttons_config configs[4] = {
		{
			.pin_number = BOARD_PIN_BTN_0,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = bm_button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_1,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = bm_button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_2,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = bm_button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_3,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = bm_button_handler,
		},
	};

	err = bm_buttons_init(configs, ARRAY_SIZE(configs), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		return err;
	}

	err = bm_buttons_enable();

	return err;
}


int main(void)
{
	int err;
	bool initialized = false;

	printk("NFC Tag TNEP application\n");

	leds_init();

	err = board_buttons_init();
	if (err) {
		printk("buttons init error %d", err);
		goto fail;
	}

#if defined(CONFIG_SOFTDEVICE)
	/* To be able to control HFCLK through SoftDevice what is required by
	 * CONFIG_BM_NFC_PLATFORM the SoftDevice needs to be enabled prior to
	 * NFC field detection start.
	 */
	err = nrf_sdh_enable_request();

	if (err) {
		LOG_ERR("Failed to enable SoftDevice, err %d", err);
		goto fail;
	}

	LOG_INF("SoftDevice enabled");
	while (LOG_PROCESS()) {
	}
#endif /* defined(CONFIG_SOFTDEVICE) */

	/* TNEP init */
	err = nfc_tnep_tag_tx_msg_buffer_register(tag_buffer, tag_buffer2,
						  sizeof(tag_buffer));
	if (err) {
		printk("Cannot register tnep buffer, err: %d\n", err);
		goto fail;
	}

	err = nfc_tnep_tag_signalling_init();
	if (err) {
		printk("Cannot initialize TNEP signalling, err: %d\n", err);
		goto fail;
	}

	err = nfc_tnep_tag_init(nfc_t4t_ndef_rwpayload_set);
	if (err) {
		printk("Cannot initialize TNEP protocol, err: %d\n", err);
		goto fail;
	}

	/* Type 4 Tag setup */
	err = nfc_t4t_setup(nfc_callback, NULL);
	if (err) {
		printk("NFC T4T setup err: %d\n", err);
		goto fail;
	}

	err = nfc_tnep_tag_initial_msg_create(TNEP_INITIAL_MSG_RECORD_COUNT,
					      tnep_initial_msg_encode);
	if (err) {
		printk("Cannot create initial TNEP message, err: %d\n", err);
	}

	err = nfc_t4t_emulation_start();
	if (err) {
		printk("NFC T4T emulation start err: %d\n", err);
		goto fail;
	}

	led_init_on();
	initialized = true;

fail:
	/* Main loop */
	while (true) {
		while (LOG_PROCESS()) {
		}

		if (initialized) {
			nfc_tnep_tag_process();
		}

		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
	}
}
