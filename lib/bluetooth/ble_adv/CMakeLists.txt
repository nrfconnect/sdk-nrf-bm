#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
zephyr_library()
zephyr_library_sources_ifdef(CONFIG_BLE_ADV ble_adv.c)
zephyr_library_sources_ifdef(CONFIG_BLE_ADV_DATA ble_adv_data.c)
