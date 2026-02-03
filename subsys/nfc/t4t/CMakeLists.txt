#
# Copyright (c) 2026 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_BM_NFC_T4T_NDEF_FILE)
zephyr_sources_ifdef(CONFIG_BM_NFC_T4T_NDEF_FILE ndef_file.c)
endif()
