#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bm_zms)

target_include_directories(app PRIVATE
			   ${ZEPHYR_NRF_BM_MODULE_DIR}/include
			   ${ZEPHYR_NRF_BM_MODULE_DIR}/subsys/fs/bm_zms
			   )

target_sources(app PRIVATE src/main.c)
