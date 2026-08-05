#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library_sources(
  conn_state.c
  gatt_cache_manager.c
  gatts_cache_manager.c
  id_manager.c
  peer_data_storage.c
  peer_database.c
  peer_id.c
  peer_manager_handler.c
  pm_buffer.c
  security_dispatcher.c
  security_manager.c
)

# zephyr-keep-sorted-start
zephyr_library_sources_ifdef(CONFIG_PM_LESC nrf_ble_lesc.c)
zephyr_library_sources_ifdef(CONFIG_PM_RA_PROTECTION auth_status_tracker.c)
# zephyr-keep-sorted-stop
