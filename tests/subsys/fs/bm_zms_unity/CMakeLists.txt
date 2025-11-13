#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(bm_zms_unity)

cmock_handle(${ZEPHYR_BASE}/include/zephyr/sys/crc.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/storage/bm_storage.h)

target_include_directories(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/include)

target_compile_definitions(app PRIVATE
        CONFIG_BM_ZMS_MAX_USERS=8
        CONFIG_BM_ZMS_OP_QUEUE_SIZE=16
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

# Unit under test
target_sources(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/subsys/fs/bm_zms/bm_zms.c)
