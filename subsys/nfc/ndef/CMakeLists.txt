#
# Copyright (c) 2026 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_BM_NFC_NDEF_MSG)
zephyr_library()
zephyr_library_sources_ifdef(CONFIG_BM_NFC_NDEF_MSG msg.c)
zephyr_library_sources_ifdef(CONFIG_BM_NFC_NDEF_RECORD record.c)
zephyr_library_sources_ifdef(CONFIG_BM_NFC_NDEF_TEXT_RECORD text_rec.c)
zephyr_library_sources_ifdef(CONFIG_BM_NFC_NDEF_URI_MSG uri_msg.c)
zephyr_library_sources_ifdef(CONFIG_BM_NFC_NDEF_URI_REC uri_rec.c)
zephyr_library_sources_ifdef(CONFIG_BM_NFC_NDEF_LAUNCHAPP_MSG launchapp_msg.c)
zephyr_library_sources_ifdef(CONFIG_BM_NFC_NDEF_LAUNCHAPP_REC launchapp_rec.c)
endif()
