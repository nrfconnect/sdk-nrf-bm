#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(bm_fifo)

target_include_directories(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/include)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

# Unit under test
target_sources(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/lib/bm_fifo/bm_fifo.c)
