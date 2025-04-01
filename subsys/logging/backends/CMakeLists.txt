#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_sources_ifdef(
  CONFIG_LOG_BACKEND_LITE_UARTE
  log_backend_lite_uarte.c
)
