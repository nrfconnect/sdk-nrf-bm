#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
add_subdirectory_ifdef(CONFIG_BLE_BAS ble_bas)
add_subdirectory_ifdef(CONFIG_BLE_BMS ble_bms)
add_subdirectory_ifdef(CONFIG_BLE_CGMS ble_cgms)
add_subdirectory_ifdef(CONFIG_BLE_DIS ble_dis)
add_subdirectory_ifdef(CONFIG_BLE_HIDS ble_hids)
add_subdirectory_ifdef(CONFIG_BLE_HRS ble_hrs)
add_subdirectory_ifdef(CONFIG_BLE_LBS ble_lbs)
add_subdirectory_ifdef(CONFIG_BLE_NUS ble_nus)
add_subdirectory_ifdef(CONFIG_BLE_NUS_CLIENT ble_nus_client)
add_subdirectory_ifdef(CONFIG_BLE_MCUMGR ble_mcumgr)
add_subdirectory_ifdef(CONFIG_BLE_HRS_CLIENT ble_hrs_client)
