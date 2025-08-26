/**
 * Copyright (c) 2012 - 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_adv BLE advertising library
 * @{
 * @brief Library for handling connectable BLE advertising.
 *
 *  The BLE advertising library supports only applications with a single peripheral link.
 */

#ifndef BLE_ADV_H__
#define BLE_ADV_H__

#include <stdint.h>
#include <stdbool.h>
#include <ble_adv_data.h>
#include <ble.h>
#include <ble_gap.h>
#include <ble_gattc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advertising module BLE event observer priority.
 */
#define BLE_ADV_BLE_OBSERVER_PRIO 0

/**
 * @brief Declare an instance of a BLE advertising library.
 */
#define BLE_ADV_DEF(instance)                                                                      \
	static struct ble_adv instance;                                                            \
	NRF_SDH_BLE_OBSERVER(ble_adv_##instance, ble_adv_on_ble_evt, &instance,                    \
			     BLE_ADV_BLE_OBSERVER_PRIO)

/**
 * @brief Advertising modes.
 */
enum ble_adv_mode {
	/**
	 * @brief Idle; non-connectable advertising is ongoing.
	 */
	BLE_ADV_MODE_IDLE,
	/**
	 * @brief  Directed advertising (high duty cycle).
	 *
	 * Attempt to connect to the most recently disconnected peer.
	 */
	BLE_ADV_MODE_DIRECTED_HIGH_DUTY,
	/**
	 * @brief Directed advertising (low duty cycle).
	 *
	 * Attempt to connect to the most recently disconnected peer.
	 */
	BLE_ADV_MODE_DIRECTED,
	/**
	 * @brief Fast advertising.
	 *
	 * Attempt to connect to any peer device, or filter with a whitelist if one exists.
	 */
	BLE_ADV_MODE_FAST,
	/**
	 * @brief  Slow advertising.
	 *
	 * Similar to fast advertising. By default it uses a longer advertising interval and
	 * time-out than fast advertising. However, these options can be adjusted by the user.
	 */
	BLE_ADV_MODE_SLOW,
};

/**
 * @brief Advertising event types.
 */
enum ble_adv_evt_type {
	/**
	 * @brief Error.
	 */
	BLE_ADV_EVT_ERROR,
	/**
	 * @brief Idle; no connectable advertising is ongoing.
	 */
	BLE_ADV_EVT_IDLE,
	/**
	 * @brief Directed advertising mode ((high duty cycle) has started.
	 */
	BLE_ADV_EVT_DIRECTED_HIGH_DUTY,
	/**
	 * @brief Directed advertising has started.
	 */
	BLE_ADV_EVT_DIRECTED,
	/**
	 * @brief Fast advertising mode has started.
	 */
	BLE_ADV_EVT_FAST,
	/**
	 * @brief Slow advertising mode has started.
	 */
	BLE_ADV_EVT_SLOW,
	/**
	 * @brief Fast advertising mode using the whitelist has started.
	 */
	BLE_ADV_EVT_FAST_WHITELIST,
	/**
	 * @brief Slow advertising mode using the whitelist has started.
	 */
	BLE_ADV_EVT_SLOW_WHITELIST,
	/**
	 * @brief Whitelist request.
	 *
	 * When this event is received, the application can reply with a whitelist to be used
	 * for advertising by calling @ref ble_adv_whitelist_reply. Otherwise, it can
	 * ignore the event to let the device advertise without a whitelist.
	 */
	BLE_ADV_EVT_WHITELIST_REQUEST,
	/**
	 * @brief Peer address request (for directed advertising).
	 *
	 * When this event is received, the application can reply with a peer address to be used
	 * for directed advertising by calling @ref ble_adv_peer_addr_reply. Otherwise, it can
	 * ignore the event to let the device advertise in the next configured advertising mode.
	 */
	BLE_ADV_EVT_PEER_ADDR_REQUEST
};

/** @brief Advertising event. */
struct ble_adv_evt {
	/** @brief Advertising event type. */
	enum ble_adv_evt_type evt_type;
	union {
		/** @ref BLE_ADV_EVT_ERROR event data. */
		struct {
			int reason;
		} error;
	};
};

/** Forward definition of ble_adv struct */
struct ble_adv;

/**
 * @brief BLE advertising event handler.
 */
typedef void (*ble_adv_evt_handler_t)(struct ble_adv *adv, const struct ble_adv_evt *adv_evt);

/**
 * @brief BLE advertising instance.
 */
struct ble_adv {
	/**
	 * @brief Initialization flag.
	 */
	bool is_initialized;
	/**
	 * @brief Current advertising mode.
	 */
	enum ble_adv_mode mode_current;
	/**
	 * @brief The connection settings used if the advertising result in a connection.
	 */
	uint8_t conn_cfg_tag;
	/**
	 * @brief Advertising handle.
	 */
	uint8_t adv_handle;
	/**
	 * @brief BLE connection handle.
	 */
	uint16_t conn_handle;
	/**
	 * @brief Instance event handler.
	 */
	ble_adv_evt_handler_t evt_handler;
	/**
	 * @brief GAP advertising parameters.
	 */
	ble_gap_adv_params_t adv_params;

#ifdef BLE_GAP_ADV_SET_DATA_SIZE_EXTENDED_CONNECTABLE_MAX_SUPPORTED
	/** Advertising data sets in encoded form. Current and swap buffer */
	uint8_t enc_adv_data[2][BLE_GAP_ADV_SET_DATA_SIZE_EXTENDED_CONNECTABLE_MAX_SUPPORTED];
	/** Scan response data sets in encoded form. Current and swap buffer */
	uint8_t enc_scan_rsp_data[2][BLE_GAP_ADV_SET_DATA_SIZE_EXTENDED_CONNECTABLE_MAX_SUPPORTED];
#else
	/** Advertising data sets in encoded form. Current and swap buffer */
	uint8_t enc_adv_data[2][BLE_GAP_ADV_SET_DATA_SIZE_MAX];
	/** Scan response data sets in encoded form. Current and swap buffer */
	uint8_t enc_scan_rsp_data[2][BLE_GAP_ADV_SET_DATA_SIZE_MAX];
#endif /* BLE_GAP_ADV_SET_DATA_SIZE_EXTENDED_CONNECTABLE_MAX_SUPPORTED */
	/**
	 * @brief Advertising data.
	 */
	ble_gap_adv_data_t adv_data;

	/**
	 * @brief GAP address to use for directed advertising.
	 */
	ble_gap_addr_t peer_address;
	/**
	 * @brief Whether a peer address has been requested.
	 */
	bool peer_addr_reply_expected;
	/**
	 * @brief Whether a whitelist has been requested.
	 */
	bool whitelist_reply_expected;
	/**
	 * @brief Whether the whitelist is disabled.
	 */
	bool whitelist_temporarily_disabled;
	/**
	 * @brief Whether the whitelist is in use.
	 */
	bool whitelist_in_use;
};

/**
 * @brief Advertising library initialization parameters.
 */
struct ble_adv_config {
	/**
	 * @brief Advertising data: name, appearance, discovery flags, and more.
	 */
	struct ble_adv_data adv_data;
	/**
	 * @brief Scan response data: Supplement to advertising data.
	 */
	struct ble_adv_data sr_data;
	 /**
	  * @brief  Event handler.
	  */
	ble_adv_evt_handler_t evt_handler;
	/**
	 * @brief Connection configuration tag.
	 */
	uint8_t conn_cfg_tag;
};

/**
 * @brief Library's BLE event handler.
 *
 * @param[in] ble_evt BLE stack event.
 * @param[in] ble_adv Advertising Module instance.
 */
void ble_adv_on_ble_evt(const ble_evt_t *ble_evt, void *ble_adv);

/**
 * @brief Initialize the BLE advertising library.
 *
 * @param[in] ble_adv BLE advertising instance.
 * @param[in] ble_adv_config Initialization configuration.
 *
 * @retval 0 On success.
 * @retval -FAULT If @p ble_adv or @p ble_adv_config are @c NULL.
 * @retval -EINVAL If the configuration @p ble_adv_config is invalid.
 */
int ble_adv_init(struct ble_adv *ble_adv, struct ble_adv_config *ble_adv_config);

/**
 * @brief Set the connection configuration tag used for connections.
 *
 * @param[in] ble_adv BLE advertising instance.
 * @param[in] ble_cfg_tag Connection configuration tag.
 */
int ble_adv_conn_cfg_tag_set(struct ble_adv *ble_adv, uint8_t ble_cfg_tag);

/**
 * @brief Start advertising in given mode.
 *
 * If the given advertising mode @p mode is not enabled,
 * advertising is started in the next supported mode.
 *
 * @param[in] ble_adv BLE advertising instance.
 * @param[in] mode Desired advertising mode.
 *
 * @retval 0 On success.
 * @retval -EPERM  Library is not initialized.
 * @retval -EFAULT @p ble_adv is @c NULL.
 * @retval -EINVAL Invalid parameters.
 */
int ble_adv_start(struct ble_adv *ble_adv, enum ble_adv_mode mode);

/**
 * @brief Set the peer address for directed advertising.
 *
 * The peer address can be set by the application upon receiving a
 * @ref BLE_ADV_EVT_PEER_ADDR_REQUEST event. If the application does not reply
 * with a peer address, the device starts advertising in the next advertising mode.
 *
 * @param[in] p_advertising Advertising Module instance.
 * @param[in] p_peer_addr   Pointer to a peer address.
 *
 * @retval 0 On success.
 * @retval -EPERM  Library is not initialized.
 * @retval -EFAULT @p ble_adv is @c NULL.
 * @retval -EINVAL Invalid parameters.
 */
int ble_adv_peer_addr_reply(struct ble_adv *ble_adv, const ble_gap_addr_t *peer_addr);

/**
 * @brief Set a whitelist for fast and slow advertising.
 *
 * The whitelist must be set by the application upon receiving @ref BLE_ADV_EVT_WHITELIST_REQUEST
 * Without the whitelist, the whitelist advertising for fast and slow modes will not be run.
 *
 * @param[in] ble_adv Advertising Module instance.
 * @param[in] gap_addrs The list of GAP addresses to whitelist.
 * @param[in] addr_cnt The number of GAP addresses to whitelist.
 * @param[in] gap_irks The list of peer IRK to whitelist.
 * @param[in] irk_cnt The number of peer IRK to whitelist.
 *
 * @retval 0 On success.
 * @retval -EPERM  Library is not initialized.
 * @retval -EFAULT @p ble_adv is @c NULL.
 * @retval -EINVAL Invalid parameters.
 */
int ble_adv_whitelist_reply(struct ble_adv *ble_adv,
			    const ble_gap_addr_t *gap_addrs, uint32_t addr_cnt,
			    const ble_gap_irk_t *gap_irks, uint32_t irk_cnt);

/**
 * @brief Restart advertising without whitelist.
 *
 * This function temporarily disables whitelist advertising.
 * Calling this function resets the current time-out countdown.
 *
 * @param[in] ble_adv Advertising Module instance.
 *
 * @retval 0 On success.
 * @retval -EPERM  Library is not initialized.
 * @retval -EFAULT @p ble_adv is @c NULL.
 */
int ble_adv_restart_without_whitelist(struct ble_adv *ble_adv);

/**@brief   Function for updating advertising data.
 *
 * @details This function can be called if you wish to reconfigure the advertising data The update
 *          will be effective even if advertising has already been started.
 *
 * @param[in]  p_advertising Advertising Module instance.
 * @param[in]  p_advdata     Pointer to the structure for specifying the content of advertising
 * data. Or null if there should be no advertising data.
 * @param[in]  p_srdata      Pointer to the structure for specifying the content of scan response
 * data. Or null if there should be no advertising data.
 *
 * @retval @ref NRF_ERROR_NULL          If advertising instance was null.
 *                                      If both \p p_advdata and \p p_srdata are null.
 * @retval @ref NRF_ERROR_INVALID_STATE If advertising instance was not initialized.
 * @retval @ref NRF_SUCCESS or any error from @ref ble_advdata_encode or
 *         @ref sd_ble_gap_adv_set_configure().
 */
int ble_adv_data_update(struct ble_adv *ble_adv,
			const struct ble_adv_data *adv,
			const struct ble_adv_data *sr);

#ifdef __cplusplus
}
#endif

#endif /* BLE_ADV_H__ */

/** @} */
