#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library_named(sdh_evt_dispatch)

zephyr_include_directories(include)
zephyr_library_sources(sdh_evt_dispatch.c)

zephyr_linker_sources(SECTIONS sdh_evt_dispatch.ld)
