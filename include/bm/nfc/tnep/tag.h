/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BM_NFC_TNEP_TAG_H_
#define BM_NFC_TNEP_TAG_H_

#include <nfc/tnep/tag.h>

#if defined(CONFIG_NFC_TNEP_TAG_SIGNALLING_BM)

/**
 * @brief Start communication using TNEP.
 *
 * @param[in] payload_set Function for setting NDEF data for NFC TNEP
 *                        Tag Device. This library use it internally
 *                        to set raw NDEF message to the Tag NDEF file.
 *                        This function is called from atomic context, so
 *                        sleeping or anyhow delaying is not allowed there.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int nfc_tnep_tag_init(nfc_payload_set_t payload_set);

#endif /* CONFIG_NFC_TNEP_TAG_SIGNALLING_BM */

#endif /* BM_NFC_TNEP_TAG_H_ */
