#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

add_subdirectory_ifdef(CONFIG_BLE_ADV ble_adv)
add_subdirectory_ifdef(CONFIG_BLE_CONN_PARAMS ble_conn_params)
add_subdirectory_ifdef(CONFIG_BLE_CONN_STATE ble_conn_state)
add_subdirectory_ifdef(CONFIG_BLE_GATT_QUEUE ble_gq)
add_subdirectory_ifdef(CONFIG_BLE_RACP ble_racp)
add_subdirectory_ifdef(CONFIG_BLE_RADIO_NOTIFICATION ble_radio_notification)
add_subdirectory_ifdef(CONFIG_BLE_SCAN ble_scan)
add_subdirectory_ifdef(CONFIG_BLE_QWR ble_qwr)
add_subdirectory_ifdef(CONFIG_PEER_MANAGER peer_manager)
