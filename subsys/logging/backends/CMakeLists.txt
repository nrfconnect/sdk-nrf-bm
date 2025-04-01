#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_sources_ifdef(
  CONFIG_LOG_BACKEND_BM_UARTE
  log_backend_bm_uarte.c
)
