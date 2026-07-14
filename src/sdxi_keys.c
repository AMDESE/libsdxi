// SPDX-License-Identifier: BSD-3-Clause
#include "libsdxi.h"

#include <errno.h>
#include <string.h>

static uint32_t sdxi_cpu_to_le32(uint32_t v)
{
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
	defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return v;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return __builtin_bswap32(v);
#else
#error "Unsupported byte order"
#endif
#else
	return v;
#endif
}

static int sdxi_key_hdr_pack(uint16_t ctrl, uint16_t intr_num, uint16_t *out)
{
	uint16_t hdr = 0;

	if (!out)
		return -EINVAL;
	*out = 0;

	if (ctrl & ~(uint16_t)(SDXI_KEY_CTRL_VL | SDXI_KEY_CTRL_IV |
			       SDXI_KEY_CTRL_PV | SDXI_KEY_CTRL_STE))
		return -EINVAL;
	if (intr_num > 0x7ffu)
		return -EINVAL;

	if (ctrl & SDXI_KEY_CTRL_VL)
		hdr |= (uint16_t)(1u << 0);
	if (ctrl & SDXI_KEY_CTRL_IV)
		hdr |= (uint16_t)(1u << 1);
	if (ctrl & SDXI_KEY_CTRL_PV)
		hdr |= (uint16_t)(1u << 2);
	if (ctrl & SDXI_KEY_CTRL_STE)
		hdr |= (uint16_t)(1u << 3);

	hdr |= (uint16_t)((intr_num & 0x7ffu) << 4);

	*out = hdr;
	return 0;
}

void sdxi_key_payload_clear(struct sdxi_key_payload *payload)
{
	if (!payload)
		return;

	memset(payload, 0, sizeof(*payload));
}

int sdxi_rkey_payload_encode(struct sdxi_key_payload *payload,
			     uint16_t req_sfunc, uint16_t ctrl,
			     uint16_t intr_num, uint32_t pasid, bool pr,
			     uint8_t ph, uint16_t stag)
{
	uint16_t hdr;
	uint32_t dw0, dw1, dw2;
	int ret;

	if (!payload)
		return -EINVAL;
	sdxi_key_payload_clear(payload);

	if (!(ctrl & SDXI_KEY_CTRL_VL))
		return 0;

	if (req_sfunc == 0)
		return -EINVAL;
	if (ph > 0x3u)
		return -EINVAL;
	if (pasid > 0xfffffu)
		return -EINVAL;
	if (!(ctrl & SDXI_KEY_CTRL_IV) && intr_num != 0)
		return -EINVAL;
	if (!(ctrl & SDXI_KEY_CTRL_PV) && pasid != 0)
		return -EINVAL;
	if (!(ctrl & SDXI_KEY_CTRL_STE) && (stag != 0 || ph != 0))
		return -EINVAL;

	ret = sdxi_key_hdr_pack(ctrl, intr_num, &hdr);
	if (ret)
		return ret;

	dw0 = (uint32_t)hdr | ((uint32_t)req_sfunc << 16);
	dw1 = (uint32_t)(pasid & 0xfffffu);
	if (pr)
		dw1 |= (uint32_t)(1u << 29);
	dw1 |= (uint32_t)((ph & 0x3u) << 30);
	dw2 = (uint32_t)stag;

	payload->dword[0] = sdxi_cpu_to_le32(dw0);
	payload->dword[1] = sdxi_cpu_to_le32(dw1);
	payload->dword[2] = sdxi_cpu_to_le32(dw2);
	payload->dword[3] = 0;
	return 0;
}

int sdxi_akey_payload_encode(struct sdxi_key_payload *payload,
			     uint16_t tgt_sfunc, uint16_t ctrl,
			     uint16_t intr_num, uint32_t pasid, bool pr,
			     uint8_t ph, uint16_t stag, uint16_t rkey)
{
	uint16_t hdr;
	uint32_t dw0, dw1, dw2, dw3;
	int ret;

	if (!payload)
		return -EINVAL;
	sdxi_key_payload_clear(payload);

	if (!(ctrl & SDXI_KEY_CTRL_VL))
		return 0;

	if (ph > 0x3u)
		return -EINVAL;
	if (pasid > 0xfffffu)
		return -EINVAL;

	if (tgt_sfunc != 0) {
		/* Remote: only VL is valid; other fields are reserved. */
		if (ctrl != SDXI_KEY_CTRL_VL)
			return -EINVAL;
		if (intr_num != 0 || pasid != 0 || pr || ph != 0 || stag != 0)
			return -EINVAL;

		ret = sdxi_key_hdr_pack(ctrl, 0, &hdr);
		if (ret)
			return ret;

		dw0 = (uint32_t)hdr | ((uint32_t)tgt_sfunc << 16);
		dw1 = 0;
		dw2 = 0;
		dw3 = (uint32_t)rkey;

		payload->dword[0] = sdxi_cpu_to_le32(dw0);
		payload->dword[1] = 0;
		payload->dword[2] = 0;
		payload->dword[3] = sdxi_cpu_to_le32(dw3);
		return 0;
	}

	/* Local: rkey is reserved (must be zero). */
	if (rkey != 0)
		return -EINVAL;
	if (!(ctrl & SDXI_KEY_CTRL_IV) && intr_num != 0)
		return -EINVAL;
	if (!(ctrl & SDXI_KEY_CTRL_PV) && pasid != 0)
		return -EINVAL;
	if (!(ctrl & SDXI_KEY_CTRL_STE) && (stag != 0 || ph != 0))
		return -EINVAL;

	ret = sdxi_key_hdr_pack(ctrl, intr_num, &hdr);
	if (ret)
		return ret;

	dw0 = (uint32_t)hdr; /* tgt_sfunc is 0 */
	dw1 = (uint32_t)(pasid & 0xfffffu);
	if (pr)
		dw1 |= (uint32_t)(1u << 29);
	dw1 |= (uint32_t)((ph & 0x3u) << 30);
	dw2 = (uint32_t)stag;
	dw3 = 0;

	payload->dword[0] = sdxi_cpu_to_le32(dw0);
	payload->dword[1] = sdxi_cpu_to_le32(dw1);
	payload->dword[2] = sdxi_cpu_to_le32(dw2);
	payload->dword[3] = 0;
	return 0;
}
