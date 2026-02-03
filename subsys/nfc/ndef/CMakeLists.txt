#
# Copyright (c) 2026 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_BM_NFC_NDEF_LE_OOB_REC le_oob_rec.c)
zephyr_library_sources_ifdef(CONFIG_BM_NFC_NDEF_CH_MSG ch_msg.c)
