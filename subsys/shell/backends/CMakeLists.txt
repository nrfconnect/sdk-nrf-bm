#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_sources_ifdef(
  CONFIG_SHELL_BACKEND_BM_UARTE
  bm_uarte.c
)
