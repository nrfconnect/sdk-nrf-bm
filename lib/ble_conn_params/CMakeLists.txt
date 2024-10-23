#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
zephyr_library()
zephyr_library_sources(event.c)
zephyr_library_sources_ifdef(CONFIG_BLE_CONN_PARAMS_AUTO_GAP_CONN_PARAM_UPDATE conn_param.c)
zephyr_library_sources_ifdef(CONFIG_BLE_CONN_PARAMS_AUTO_ATT_MTU att_mtu.c)
zephyr_library_sources_ifdef(CONFIG_BLE_CONN_PARAMS_AUTO_DATA_LENGTH data_length.c)
