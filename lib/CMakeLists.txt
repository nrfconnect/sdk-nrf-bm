#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

add_subdirectory_ifdef(CONFIG_BLE_ADV ble_adv)
add_subdirectory_ifdef(CONFIG_BLE_CONN_PARAMS ble_conn_params)
