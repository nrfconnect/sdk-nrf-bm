#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
zephyr_sources_ifdef(CONFIG_BLE_BAS bas.c)
zephyr_sources_ifdef(CONFIG_BLE_NUS nus.c)
zephyr_sources_ifdef(CONFIG_BLE_DIS dis.c)
zephyr_sources_ifdef(CONFIG_BLE_HRS hrs.c)
zephyr_sources_ifdef(CONFIG_BLE_LBS lbs.c)
