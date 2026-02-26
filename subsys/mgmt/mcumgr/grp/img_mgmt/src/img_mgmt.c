/*
 * Copyright (c) 2018-2021 mcumgr authors
 * Copyright (c) 2022-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/util.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/toolchain.h>
#include <zephyr/logging/log.h>
#include <zephyr/dfu/mcuboot.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <zephyr/mgmt/mcumgr/mgmt/handlers.h>
#include <img_mgmt.h>

#include <mgmt/mcumgr/util/zcbor_bulk.h>
#include <mgmt/mcumgr/grp/img_mgmt/img_mgmt_priv.h>

LOG_MODULE_REGISTER(mcumgr_img_grp, CONFIG_MCUMGR_GRP_IMG_LOG_LEVEL);

struct img_mgmt_state g_img_mgmt_state;
#define ERASED_VAL 0xFF

#ifdef CONFIG_MCUMGR_GRP_IMG_VERBOSE_ERR
const char *img_mgmt_err_str_app_reject = "app reject";
const char *img_mgmt_err_str_hdr_malformed = "header malformed";
const char *img_mgmt_err_str_magic_mismatch = "magic mismatch";
const char *img_mgmt_err_str_no_slot = "no slot";
const char *img_mgmt_err_str_flash_open_failed = "fa open fail";
const char *img_mgmt_err_str_flash_erase_failed = "fa erase fail";
const char *img_mgmt_err_str_flash_write_failed = "fa write fail";
const char *img_mgmt_err_str_downgrade = "downgrade";
const char *img_mgmt_err_str_image_bad_flash_addr = "img addr mismatch";
const char *img_mgmt_err_str_image_too_large = "img too large";
const char *img_mgmt_err_str_data_overrun = "data overrun";
#endif

/**
 * Finds the TLVs in the specified image slot, if any.
 */
static int img_mgmt_find_tlvs(size_t *start_off, size_t *end_off, uint16_t magic)
{
	struct image_tlv_info tlv_info;
	int rc;

	rc = img_mgmt_read(*start_off, &tlv_info, sizeof(tlv_info));
	if (rc != 0) {
		/* Read error. */
		return rc;
	}

	if (tlv_info.it_magic != magic) {
		/* No TLVs. */
		return IMG_MGMT_ERR_NO_TLVS;
	}

	*start_off += sizeof(tlv_info);
	*end_off = *start_off + tlv_info.it_tlv_tot;

	return IMG_MGMT_ERR_OK;
}

/*
 * Reads the version and build hash from the specified image slot.
 */
int img_mgmt_read_info(int image_slot, struct image_version *ver, uint8_t *hash, uint32_t *flags)
{
	struct image_header hdr;
	struct image_tlv tlv;
	size_t data_off;
	size_t data_end;
	bool hash_found;
	uint32_t erased_val_32;
	int rc;

	rc = img_mgmt_read(0,
			   &hdr, sizeof(hdr));
	LOG_ERR("read info - %d", rc);

	if (rc != 0) {
		return rc;
	}

	if (ver != NULL) {
		memset(ver, ERASED_VAL, sizeof(*ver));
	}
	erased_val_32 = ERASED_VAL_32(ERASED_VAL);
	if (hdr.ih_magic == IMAGE_MAGIC) {
		if (ver != NULL) {
			memcpy(ver, &hdr.ih_ver, sizeof(*ver));
		}
	} else if (hdr.ih_magic == erased_val_32) {
		return IMG_MGMT_ERR_NO_IMAGE;
	} else {
		return IMG_MGMT_ERR_INVALID_IMAGE_HEADER_MAGIC;
	}

	if (flags != NULL) {
		*flags = hdr.ih_flags;
	}

	/* Read the image's TLVs. We first try to find the protected TLVs, if the protected
	 * TLV does not exist, we try to find non-protected TLV which also contains the hash
	 * TLV. All images are required to have a hash TLV.  If the hash is missing, the image
	 * is considered invalid.
	 */
	data_off = hdr.ih_hdr_size + hdr.ih_img_size;

	rc = img_mgmt_find_tlvs(&data_off, &data_end, IMAGE_TLV_PROT_INFO_MAGIC);
	if (!rc) {
		/* The data offset should start after the header bytes after the end of
		 * the protected TLV, if one exists.
		 */
		data_off = data_end - sizeof(struct image_tlv_info);
	}

	rc = img_mgmt_find_tlvs(&data_off, &data_end, IMAGE_TLV_INFO_MAGIC);
	if (rc != 0) {
		return IMG_MGMT_ERR_NO_TLVS;
	}

	hash_found = false;
	while (data_off + sizeof(tlv) <= data_end) {
		rc = img_mgmt_read(data_off, &tlv, sizeof(tlv));
		if (rc != 0) {
			return rc;
		}
		if (tlv.it_type == 0xff && tlv.it_len == 0xffff) {
			return IMG_MGMT_ERR_INVALID_TLV;
		}
		if (tlv.it_type != IMAGE_TLV_SHA || tlv.it_len != IMAGE_SHA_LEN) {
			/* Non-hash TLV.  Skip it. */
			data_off += sizeof(tlv) + tlv.it_len;
			continue;
		}

		if (hash_found) {
			/* More than one hash. */
			return IMG_MGMT_ERR_TLV_MULTIPLE_HASHES_FOUND;
		}
		hash_found = true;

		data_off += sizeof(tlv);
		if (hash != NULL) {
			if (data_off + IMAGE_SHA_LEN > data_end) {
				return IMG_MGMT_ERR_TLV_INVALID_SIZE;
			}
			rc = img_mgmt_read(data_off, hash, IMAGE_SHA_LEN);
			if (rc != 0) {
				return rc;
			}
		}
	}

	if (!hash_found) {
		return IMG_MGMT_ERR_HASH_NOT_FOUND;
	}

	return 0;
}

/*
 * Resets upload status to defaults (no upload in progress)
 */
static void img_mgmt_reset_upload(void)
{
	memset(&g_img_mgmt_state, 0, sizeof(g_img_mgmt_state));
	g_img_mgmt_state.area_id = -1;
}

static int
img_mgmt_upload_good_rsp(struct smp_streamer *ctxt)
{
	zcbor_state_t *zse = ctxt->writer->zs;
	bool ok = true;

	if (IS_ENABLED(CONFIG_MCUMGR_SMP_LEGACY_RC_BEHAVIOUR)) {
		ok = zcbor_tstr_put_lit(zse, "rc")		&&
		     zcbor_int32_put(zse, MGMT_ERR_EOK);
	}

	ok = ok && zcbor_tstr_put_lit(zse, "off")		&&
		   zcbor_size_put(zse, g_img_mgmt_state.off);

	return ok ? MGMT_ERR_EOK : MGMT_ERR_EMSGSIZE;
}


/**
 * Command handler: image upload
 */
static int
img_mgmt_upload(struct smp_streamer *ctxt)
{
	zcbor_state_t *zse = ctxt->writer->zs;
	zcbor_state_t *zsd = ctxt->reader->zs;
	bool ok;
	size_t decoded = 0;
	struct img_mgmt_upload_req req = {
		.off = SIZE_MAX,
		.size = SIZE_MAX,
		.img_data = { 0 },
		.data_sha = { 0 },
		.upgrade = false,
		.image = 0,
	};
	int rc;
	struct img_mgmt_upload_action action;
	bool last = false;
	bool reset = false;

	struct zcbor_map_decode_key_val image_upload_decode[] = {
		ZCBOR_MAP_DECODE_KEY_DECODER("image", zcbor_uint32_decode, &req.image),
		ZCBOR_MAP_DECODE_KEY_DECODER("data", zcbor_bstr_decode, &req.img_data),
		ZCBOR_MAP_DECODE_KEY_DECODER("len", zcbor_size_decode, &req.size),
		ZCBOR_MAP_DECODE_KEY_DECODER("off", zcbor_size_decode, &req.off),
		ZCBOR_MAP_DECODE_KEY_DECODER("sha", zcbor_bstr_decode, &req.data_sha),
		ZCBOR_MAP_DECODE_KEY_DECODER("upgrade", zcbor_bool_decode, &req.upgrade)
	};

	ok = zcbor_map_decode_bulk(zsd, image_upload_decode,
		ARRAY_SIZE(image_upload_decode), &decoded) == 0;

	IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(&action, NULL);

	if (!ok) {
		return MGMT_ERR_EINVAL;
	}

	/* Determine what actions to take as a result of this request. */
	rc = img_mgmt_upload_inspect(&req, &action);
	if (rc != 0) {

		MGMT_CTXT_SET_RC_RSN(ctxt, IMG_MGMT_UPLOAD_ACTION_RC_RSN(&action));
		LOG_ERR("Image upload inspect failed: %d", rc);
		ok = smp_add_cmd_err(zse, MGMT_GROUP_ID_IMAGE, rc);
		goto end;
	}

	if (!action.proceed) {
		/* Request specifies incorrect offset.  Respond with a success code and
		 * the correct offset.
		 */
		rc = img_mgmt_upload_good_rsp(ctxt);
		return rc;
	}

	/* Remember flash area ID and image size for subsequent upload requests. */
	g_img_mgmt_state.area_id = action.area_id;
	g_img_mgmt_state.size = action.size;

	if (req.off == 0) {
		/*
		 * New upload.
		 */

		g_img_mgmt_state.off = 0;

		/*
		 * We accept SHA trimmed to any length by client since it's up to client
		 * to make sure provided data are good enough to avoid collisions when
		 * resuming upload.
		 */
		g_img_mgmt_state.data_sha_len = req.data_sha.len;
		memcpy(g_img_mgmt_state.data_sha, req.data_sha.value, req.data_sha.len);
		memset(&g_img_mgmt_state.data_sha[req.data_sha.len], 0,
			   IMG_MGMT_DATA_SHA_LEN - req.data_sha.len);


#ifndef CONFIG_IMG_ERASE_PROGRESSIVELY
		/* erase the entire req.size all at once */
		if (action.erase) {
			rc = img_mgmt_erase_image_data(0, req.size);
			if (rc != 0) {
				IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(&action,
					img_mgmt_err_str_flash_erase_failed);
				ok = smp_add_cmd_err(zse, MGMT_GROUP_ID_IMAGE, rc);
				goto end;
			}
		}
#endif
	}

	/* Write the image data to flash. */
	if (req.img_data.len != 0) {
		/* If this is the last chunk */
		if (g_img_mgmt_state.off + req.img_data.len == g_img_mgmt_state.size) {
			last = true;
		}

		rc = img_mgmt_write_image_data(req.off, req.img_data.value, action.write_bytes,
						    last);
		if (rc == 0 || rc == IMG_MGMT_ERR_BUSY) {
			if (rc == 0) {
				g_img_mgmt_state.off += action.write_bytes;
			} else {
				LOG_ERR("busy");
			}
			rc = 0;
		} else {
			/* Write failed, currently not able to recover from this */

			IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(&action,
				img_mgmt_err_str_flash_write_failed);
			reset = true;
			IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(&action,
				img_mgmt_err_str_flash_write_failed);

			LOG_ERR("Irrecoverable error: flash write failed: %d", rc);

			ok = smp_add_cmd_err(zse, MGMT_GROUP_ID_IMAGE, rc);
			goto end;
		}

		if (g_img_mgmt_state.off == g_img_mgmt_state.size) {
			/* Done */
			reset = true;
		}
	}
end:

	if (rc != 0) {

		img_mgmt_reset_upload();
	} else {
		rc = img_mgmt_upload_good_rsp(ctxt);

		if (reset) {
			/* Reset the upload state struct back to default */
			img_mgmt_reset_upload();
		}
	}

	if (!ok) {
		return MGMT_ERR_EMSGSIZE;
	}

	return MGMT_ERR_EOK;
}

int img_mgmt_my_version(struct image_version *ver)
{
	return img_mgmt_read_info(0, ver, NULL, NULL);
}

#ifdef CONFIG_MCUMGR_SMP_SUPPORT_ORIGINAL_PROTOCOL
/*
 * @brief	Translate IMG mgmt group error code into MCUmgr error code
 *
 * @param ret	#img_mgmt_err_code_t error code
 *
 * @return	#mcumgr_err_t error code
 */
static int img_mgmt_translate_error_code(uint16_t err)
{
	int rc;

	switch (err) {
	case IMG_MGMT_ERR_NO_IMAGE:
	case IMG_MGMT_ERR_NO_TLVS:
		rc = MGMT_ERR_ENOENT;
		break;

	case IMG_MGMT_ERR_NO_FREE_SLOT:
	case IMG_MGMT_ERR_CURRENT_VERSION_IS_NEWER:
	case IMG_MGMT_ERR_IMAGE_ALREADY_PENDING:
		rc = MGMT_ERR_EBADSTATE;
		break;

	case IMG_MGMT_ERR_NO_FREE_MEMORY:
		rc = MGMT_ERR_ENOMEM;
		break;

	case IMG_MGMT_ERR_INVALID_SLOT:
	case IMG_MGMT_ERR_INVALID_PAGE_OFFSET:
	case IMG_MGMT_ERR_INVALID_OFFSET:
	case IMG_MGMT_ERR_INVALID_LENGTH:
	case IMG_MGMT_ERR_INVALID_IMAGE_HEADER:
	case IMG_MGMT_ERR_INVALID_HASH:
	case IMG_MGMT_ERR_INVALID_FLASH_ADDRESS:
		rc = MGMT_ERR_EINVAL;
		break;

	case IMG_MGMT_ERR_FLASH_CONFIG_QUERY_FAIL:
	case IMG_MGMT_ERR_VERSION_GET_FAILED:
	case IMG_MGMT_ERR_TLV_MULTIPLE_HASHES_FOUND:
	case IMG_MGMT_ERR_TLV_INVALID_SIZE:
	case IMG_MGMT_ERR_HASH_NOT_FOUND:
	case IMG_MGMT_ERR_INVALID_TLV:
	case IMG_MGMT_ERR_FLASH_OPEN_FAILED:
	case IMG_MGMT_ERR_FLASH_READ_FAILED:
	case IMG_MGMT_ERR_FLASH_WRITE_FAILED:
	case IMG_MGMT_ERR_FLASH_ERASE_FAILED:
	case IMG_MGMT_ERR_FLASH_CONTEXT_ALREADY_SET:
	case IMG_MGMT_ERR_FLASH_CONTEXT_NOT_SET:
	case IMG_MGMT_ERR_FLASH_AREA_DEVICE_NULL:
	case IMG_MGMT_ERR_INVALID_IMAGE_HEADER_MAGIC:
	case IMG_MGMT_ERR_INVALID_IMAGE_VECTOR_TABLE:
	case IMG_MGMT_ERR_INVALID_IMAGE_TOO_LARGE:
	case IMG_MGMT_ERR_INVALID_IMAGE_DATA_OVERRUN:
	case IMG_MGMT_ERR_UNKNOWN:
	default:
		rc = MGMT_ERR_EUNKNOWN;
	}

	return rc;
}
#endif

static const struct mgmt_handler img_mgmt_handlers[] = {
	[IMG_MGMT_ID_STATE] = {
		.mh_read = img_mgmt_state_read,
		.mh_write = NULL
	},
	[IMG_MGMT_ID_UPLOAD] = {
		.mh_read = NULL,
		.mh_write = img_mgmt_upload
	},
};

static const struct mgmt_handler img_mgmt_handlers[];

#define IMG_MGMT_HANDLER_CNT ARRAY_SIZE(img_mgmt_handlers)

static struct mgmt_group img_mgmt_group = {
	.mg_handlers = (struct mgmt_handler *)img_mgmt_handlers,
	.mg_handlers_count = IMG_MGMT_HANDLER_CNT,
	.mg_group_id = MGMT_GROUP_ID_IMAGE,
#ifdef CONFIG_MCUMGR_SMP_SUPPORT_ORIGINAL_PROTOCOL
	.mg_translate_error = img_mgmt_translate_error_code,
#endif
};

static void img_mgmt_register_group(void)
{
	mgmt_register_group(&img_mgmt_group);
}

MCUMGR_HANDLER_DEFINE(img_mgmt, img_mgmt_register_group);
