#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

add_subdirectory_ifdef(CONFIG_BLE_ADV ble_adv)
add_subdirectory_ifdef(CONFIG_BLE_CONN_PARAMS ble_conn_params)
add_subdirectory_ifdef(CONFIG_BLE_GATT_QUEUE ble_gq)
add_subdirectory_ifdef(CONFIG_BLE_RACP ble_racp)
add_subdirectory_ifdef(CONFIG_EVENT_SCHEDULER event_scheduler)
add_subdirectory_ifdef(CONFIG_LITE_BUTTONS lite_buttons)
add_subdirectory_ifdef(CONFIG_LITE_TIMER lite_timer)
add_subdirectory_ifdef(CONFIG_BLE_QWR ble_qwr)
add_subdirectory_ifdef(CONFIG_SENSORSIM sensorsim)
add_subdirectory_ifdef(CONFIG_NCS_BARE_METAL_BOOT_BANNER boot_banner)
