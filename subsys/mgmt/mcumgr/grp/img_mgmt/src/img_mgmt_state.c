/*
 * Copyright (c) 2018-2021 mcumgr authors
 * Copyright (c) 2022-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/util_macro.h>
#include <zephyr/toolchain.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/dfu/mcuboot.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

#include <bootutil/bootutil_public.h>

#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>

#include <mgmt/mcumgr/util/zcbor_bulk.h>
#include <mgmt/mcumgr/grp/img_mgmt/img_mgmt_priv.h>

LOG_MODULE_DECLARE(mcumgr_img_grp, CONFIG_MCUMGR_GRP_IMG_LOG_LEVEL);

#ifndef CONFIG_MCUMGR_GRP_IMG_FRUGAL_LIST
#define ZCBOR_ENCODE_FLAG(zse, label, value)					\
		(zcbor_tstr_put_lit(zse, label) && zcbor_bool_put(zse, value))
#else
/* In "frugal" lists flags are added to response only when they evaluate to true */
/* Note that value is evaluated twice! */
#define ZCBOR_ENCODE_FLAG(zse, label, value)					\
		(!(value) ||							\
		 (zcbor_tstr_put_lit(zse, label) && zcbor_bool_put(zse, (value))))
#endif

/* Flags returned by img_mgmt_state_read() for queried slot */
#define REPORT_SLOT_ACTIVE	BIT(0)
#define REPORT_SLOT_PENDING	BIT(1)
#define REPORT_SLOT_CONFIRMED	BIT(2)
#define REPORT_SLOT_PERMANENT	BIT(3)

#define SLOT0 0

#if defined(CONFIG_MCUBOOT_BOOTLOADER_MODE_DIRECT_XIP_WITH_REVERT)
#define DIRECT_XIP_BOOT_UNSET		0
#define DIRECT_XIP_BOOT_ONCE		1
#define DIRECT_XIP_BOOT_REVERT		2
#define DIRECT_XIP_BOOT_FOREVER		3
#endif

#if defined(CONFIG_MCUBOOT_BOOTLOADER_MODE_FIRMWARE_UPDATER) && \
	CONFIG_MCUMGR_GRP_IMG_UPDATABLE_IMAGE_NUMBER > 1
#warning "MCUmgr img mgmt only supports 1 image"
#endif

/**
 * Collects information about the specified image slot.
 */
uint8_t
img_mgmt_state_flags(int query_slot)
{
	uint8_t flags = 0;
	int image = query_slot / 2;	/* We support max 2 images for now */

	/* In case when MCUboot is configured for FW loader/updater mode, slots
	 * can be either active or non-active. There is no concept of pending
	 * or confirmed slots.
	 *
	 * In case when MCUboot is configured for DirectXIP slot may only be
	 * active or pending.
	 * Slot is marked as pending when:
	 * - version in that slot is higher than version of active slot.
	 * - versions are equal but slot number is lower than the active slot.
	 */
	if (image == img_mgmt_active_image() && query_slot == SLOT0) {
		flags = IMG_MGMT_STATE_F_ACTIVE;
	}

	return flags;
}


/**
 * Indicates whether any image slot is pending (i.e., whether a test swap will
 * happen on the next reboot.
 */
int
img_mgmt_state_any_pending(void)
{
	return img_mgmt_state_flags(0) & IMG_MGMT_STATE_F_PENDING ||
		   img_mgmt_state_flags(1) & IMG_MGMT_STATE_F_PENDING;
}

/**
 * Indicates whether the specified slot has any flags.  If no flags are set,
 * the slot can be freely erased.
 */
int
img_mgmt_slot_in_use(int slot)
{

	return true;
}

/**
 * Sets the pending flag for the specified image slot.  That is, the system
 * will swap to the specified image on the next reboot.  If the permanent
 * argument is specified, the system doesn't require a confirm after the swap
 * occurs.
 */
int
img_mgmt_state_set_pending(int slot, int permanent)
{
	uint8_t state_flags;
	int rc;

	state_flags = img_mgmt_state_flags(slot);

	/* Unconfirmed slots are always runnable.  A confirmed slot can only be
	 * run if it is a loader in a split image setup.
	 */
	if (state_flags & IMG_MGMT_STATE_F_CONFIRMED && slot != 0) {
		rc = IMG_MGMT_ERR_IMAGE_ALREADY_PENDING;
		goto done;
	}

	rc = img_mgmt_write_pending(slot, permanent);

done:

	return rc;
}

/**
 * Confirms the current image state.  Prevents a fallback from occurring on the
 * next reboot if the active image is currently being tested.
 */
int
img_mgmt_state_confirm(void)
{
	int rc;

	/* Confirm disallowed if a test is pending. */
	if (img_mgmt_state_any_pending()) {
		rc = IMG_MGMT_ERR_IMAGE_ALREADY_PENDING;
		goto err;
	}

	rc = img_mgmt_write_confirmed();

err:
	return rc;
}

/* Return zcbor encoding result */
static bool img_mgmt_state_encode_slot(struct smp_streamer *ctxt, uint32_t slot, int state_flags)
{
	zcbor_state_t *zse = ctxt->writer->zs;
	uint32_t flags;
	char vers_str[IMG_MGMT_VER_MAX_STR_LEN];
	uint8_t hash[IMAGE_SHA_LEN];
	struct zcbor_string zhash = {
		.value = hash,
		.len = IMAGE_SHA_LEN,
	};
	struct image_version ver;
	bool ok;
	int rc = img_mgmt_read_info(slot, &ver, hash, &flags);

	if (rc != 0) {
		/* zcbor encoding did not fail */
		return true;
	}

	ok = zcbor_map_start_encode(zse, CONFIG_MCUMGR_GRP_IMG_IMAGE_SLOT_STATE_STATES)	&&
	     (CONFIG_MCUMGR_GRP_IMG_UPDATABLE_IMAGE_NUMBER == 1	||
	      (zcbor_tstr_put_lit(zse, "image")						&&
	       zcbor_uint32_put(zse, slot >> 1)))					&&
	     zcbor_tstr_put_lit(zse, "slot")						&&
	     zcbor_uint32_put(zse, slot % 2)						&&
	     zcbor_tstr_put_lit(zse, "version");

	if (ok) {
		if (img_mgmt_ver_str(&ver, vers_str) < 0) {
			ok = zcbor_tstr_put_lit(zse, "<\?\?\?>");
		} else {
			vers_str[sizeof(vers_str) - 1] = '\0';
			ok = zcbor_tstr_put_term(zse, vers_str, sizeof(vers_str));
		}
	}

	ok = ok && zcbor_tstr_put_lit(zse, "hash")					&&
	     zcbor_bstr_encode(zse, &zhash)						&&
	     ZCBOR_ENCODE_FLAG(zse, "bootable", !(flags & IMAGE_F_NON_BOOTABLE))	&&
	     ZCBOR_ENCODE_FLAG(zse, "pending", state_flags & REPORT_SLOT_PENDING)	&&
	     ZCBOR_ENCODE_FLAG(zse, "confirmed", state_flags & REPORT_SLOT_CONFIRMED)	&&
	     ZCBOR_ENCODE_FLAG(zse, "active", state_flags & REPORT_SLOT_ACTIVE)		&&
	     ZCBOR_ENCODE_FLAG(zse, "permanent", state_flags & REPORT_SLOT_PERMANENT);

	if (!ok) {
		goto failed;
	}

	ok &= zcbor_map_end_encode(zse, CONFIG_MCUMGR_GRP_IMG_IMAGE_SLOT_STATE_STATES);

failed:
	return ok;
}

/**
 * Command handler: image state read
 */
int
img_mgmt_state_read(struct smp_streamer *ctxt)
{
	zcbor_state_t *zse = ctxt->writer->zs;
	uint32_t i;
	bool ok;

	ok = zcbor_tstr_put_lit(zse, "images") &&
	     zcbor_list_start_encode(zse, 2);

	if (ok) {
		ok = img_mgmt_state_encode_slot(ctxt, SLOT0, REPORT_SLOT_ACTIVE);
	}

	/* Ending list encoding for two slots per image */
	ok = ok && zcbor_list_end_encode(zse, 2);
	/* splitStatus is always 0 so in frugal list it is not present at all */
	if (!IS_ENABLED(CONFIG_MCUMGR_GRP_IMG_FRUGAL_LIST) && ok) {
		ok = zcbor_tstr_put_lit(zse, "splitStatus") &&
		     zcbor_int32_put(zse, 0);
	}


	return ok ? MGMT_ERR_EOK : MGMT_ERR_EMSGSIZE;
}

