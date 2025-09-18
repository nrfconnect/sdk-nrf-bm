#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library_sources(auth_status_tracker.c)
zephyr_library_sources(gatt_cache_manager.c)
zephyr_library_sources(gatts_cache_manager.c)
zephyr_library_sources(id_manager.c)
zephyr_library_sources_ifdef(CONFIG_PM_LESC_ENABLED nrf_ble_lesc.c)
zephyr_library_sources(peer_data_storage.c)
zephyr_library_sources(peer_database.c)
zephyr_library_sources(peer_id.c)
zephyr_library_sources(peer_manager_handler.c)
zephyr_library_sources(pm_buffer.c)
zephyr_library_sources(security_dispatcher.c)
zephyr_library_sources(security_manager.c)
