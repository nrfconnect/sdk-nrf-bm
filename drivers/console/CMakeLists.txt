#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_sources_ifdef(
  CONFIG_LITE_UARTE_CONSOLE
  console_lite_uarte.c
)
