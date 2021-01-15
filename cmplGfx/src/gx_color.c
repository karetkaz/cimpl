// raster operations including depth conversions

#include "gx_surf.h"
#include <memory.h>

static argb defaultPalette[256];

typedef uint8_t byte;
typedef uint16_t word;

// 32 bit argb color operations (destination is 32 bit)
static int colcpy32(byte *dst, byte *src, void *lut, size_t cnt) {
	memcpy(dst, src, 4 * cnt);
	(void) lut;
	return 0;
}
static int colset32(argb *dst, void *src, argb *lut, size_t cnt) {
	for (size_t i = 0; i < cnt; ++i, ++dst) {
		*dst = *lut;
	}
	(void) src;
	return 0;
}
static int colcpy32_bgra(byte *dst, byte *src, void *lut, size_t cnt) {
	// AAAAAAAA`BBBBBBBB`GGGGGGGG`RRRRRRRR => AAAAAAAA`RRRRRRRR`GGGGGGGG`BBBBBBBB
	for (; cnt > 0; cnt -= 1, dst += 4, src += 4) {
		int32_t val = *(int32_t *) src;
		val = (val & 0xff000000)
			| ((val & 0x00ff0000) >> 16)
			| (val & 0x0000ff00)
			| ((val & 0x000000ff) << 16);
		*(int32_t *) dst = val;
	}
	(void) lut;
	return 0;
}
static int colcpy32_bgr(byte *dst, byte *src, void *lut, size_t cnt) {
	// BBBBBBBB`GGGGGGGG`RRRRRRRR => 00000000`RRRRRRRR`GGGGGGGG`BBBBBBBB
	for (; cnt > 0; cnt -= 1, dst += 4, src += 3) {
		int32_t val = *(int32_t *) src;
		val = ((val & 0xff0000) >> 16)
			| (val & 0x00ff00)
			| ((val & 0x0000ff) << 16);
		*(int32_t *) dst = val;
	}
	(void) lut;
	return 0;
}
static int colcpy32_24(argb *dst, byte *src, void *lut, size_t cnt) {
	// RRRRRRRR`GGGGGGGG`BBBBBBBB => 00000000`RRRRRRRR`GGGGGGGG`BBBBBBBB
	for (size_t i = 0; i < cnt; ++i, ++dst, src += 3) {
		int32_t val = *(int32_t *) src;
		dst->val = val & 0x00ffffff;
	}
	(void) lut;
	return 0;
}
static int colcpy32_16(argb *dst, word *src, void *lut, size_t cnt) {
	// RRRRRGGG`GGGBBBBB => 00000000`RRRRR000`GGGGGG00`BBBBB000
	for (size_t i = 0; i < cnt; ++i, ++dst, ++src) {
		int32_t val = *src;
		dst->val = ((val & 0xf800) << 8)
			| ((val & 0x07e0) << 5)
			| ((val & 0x001f) << 3);
	}
	(void) lut;
	return 0;
}
static int colcpy32_15(argb *dst, word *src, void *lut, size_t cnt) {
	// XRRRRRGG`GGGBBBBB => 00000000`RRRRR000`GGGGG000`BBBBB000
	for (size_t i = 0; i < cnt; ++i, ++dst, ++src) {
		int32_t val = *src;
		dst->val = ((val & 0x7c00) << 9)
			| ((val & 0x03e0) << 6)
			| ((val & 0x001f) << 3);
	}
	(void) lut;
	return 0;
}
static int colcpy32_08(argb *dst, byte *src, argb *lut, size_t cnt) {
	// convert to gray or use lookup table
	// IIIIIIII => 00000000`IIIIIIII`IIIIIIII`IIIIIIII
	if (lut == NULL) {
		lut = defaultPalette;
	}
	for (size_t i = 0; i < cnt; ++i, ++dst, ++src) {
		*dst = lut[*src];
	}
	return 0;
}
static int colset32_mix(argb *dst, byte *src, argb *lut, size_t cnt) {
	// src is alpha channel, lut points to the color
	argb val = *lut;
	for (size_t i = 0; i < cnt; ++i, ++dst, ++src) {
		*dst = mix_rgb(*dst, val, *src);
	}
	return 0;
}
static int colcpy32_mix(byte *dst, byte *src, int *lut, size_t cnt) {
	// lut points to the alpha value to be used
	int alpha = *lut;
	for (size_t i = 0; i < 4 * cnt; ++i, ++dst, ++src) {
		*dst = mix_s8(*dst, *src, alpha);
	}
	return 0;
}

// 24 bit bgr color operations
static int colcpy24(byte *dst, byte *src, void *lut, size_t cnt) {
	memcpy(dst, src, 3 * cnt);
	(void) lut;
	return 0;
}
static int (*const colset24)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int colcpy24_bgra(byte *dst, byte *src, void *lut, size_t cnt) {
	for (size_t i = 0; i < cnt; ++i, dst += 3, src += 4) {
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
	}
	(void) lut;
	return 0;
}
static int colcpy24_bgr(byte *dst, byte *src, void *lut, size_t cnt) {
	for (size_t i = 0; i < cnt; ++i, dst += 3, src += 3) {
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
	}
	(void) lut;
	return 0;
}
static int colcpy24_32(byte *dst, byte *src, void *lut, size_t cnt) {
	for (size_t i = 0; i < cnt; ++i, dst += 3, src += 4) {
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
	}
	(void) lut;
	return 0;
}
static int (*const colcpy24_16)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy24_15)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int colcpy24_08(byte *dst, byte *src, argb *lut, size_t cnt) {
	if (lut == NULL) {
		lut = defaultPalette;
	}
	for (size_t i = 0; i < cnt; ++i, dst += 3, ++src) {
		uint32_t val = lut[*src].val;
		dst[0] = bch(val);
		dst[1] = gch(val);
		dst[2] = rch(val);
	}
	return 0;
}
static int (*const colset24_mix)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy24_mix)(void *dst, void *src, void *lut, size_t cnt) = NULL;

// 16 bit bgr color operations
static inline word makeRGB565(int r, int g, int b) {
	return ((r << 8) & 0xf800)
		| ((g << 3) & 0x07e0)
		| ((b >> 3) & 0x001f);
}
static inline word toRGB565(argb rgb) {
	return makeRGB565(rch(rgb.val), gch(rgb.val), bch(rgb.val));
}
static int colcpy16(word *dst, word *src, void *lut, size_t cnt) {
	memcpy(dst, src, 2 * cnt);
	return 0;
	(void) lut;
}
static int colset16(word *dst, void *src, word *lut, size_t cnt) {
	for (size_t i = 0; i < cnt; ++i, ++dst) {
		*dst = *lut;
	}
	(void) src;
	return 0;
}
static int colcpy16_bgra(word *dst, byte *src, void *lut, size_t cnt) {
	// RRRRRRRR`GGGGGGGG`BBBBBBBB => RRRRRGGG`GGGBBBBB
	for (size_t i = 0; i < cnt; ++i, ++dst, src += 4) {
		*dst = makeRGB565(src[0], src[1], src[2]);
	}
	(void) lut;
	return 0;
}
static int colcpy16_bgr(word *dst, byte *src, void *lut, size_t cnt) {
	// RRRRRRRR`GGGGGGGG`BBBBBBBB => RRRRRGGG`GGGBBBBB
	for (size_t i = 0; i < cnt; ++i, ++dst, src += 3) {
		*dst = makeRGB565(src[0], src[1], src[2]);
	}
	(void) lut;
	return 0;
}
static int (*const colcpy16_32)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy16_24)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy16_15)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int colcpy16_08(word *dst, byte *src, argb *lut, size_t cnt) {
	if (lut == NULL) {
		lut = defaultPalette;
	}
	for (size_t i = 0; i < cnt; ++i, ++dst, ++src) {
		*dst = toRGB565(lut[*src]);
	}
	return 0;
}
static int (*const colset16_mix)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy16_mix)(void *dst, void *src, void *lut, size_t cnt) = NULL;

// 15 bit bgr color operations
static inline word makeRGB555(int r, int g, int b) {
	return ((r << 7) & 0x7c00)
		| ((g << 2) & 0x03e0)
		| ((b >> 3) & 0x001f);
}
static inline word toRGB555(argb rgb) {
	return makeRGB555(rch(rgb.val), gch(rgb.val), bch(rgb.val));
}
static int (*const colcpy15)(word *dst, word *src, void *lut, size_t cnt) = colcpy16;
static int (*const colset15)(word *dst, void *src, word *lut, size_t cnt) = colset16;
static int colcpy15_bgra(word *dst, byte *src, void *lut, size_t cnt) {
	// RRRRRRRR`GGGGGGGG`BBBBBBBB => XRRRRRGG`GGGBBBBB
	for (size_t i = 0; i < cnt; ++i, ++dst, src += 4) {
		*dst = makeRGB555(src[0], src[1], src[2]);
	}
	(void) lut;
	return 0;
}
static int colcpy15_bgr(word *dst, byte *src, void *lut, size_t cnt) {
	// RRRRRRRR`GGGGGGGG`BBBBBBBB => XRRRRRGG`GGGBBBBB
	for (size_t i = 0; i < cnt; ++i, ++dst, src += 3) {
		*dst = makeRGB555(src[0], src[1], src[2]);
	}
	(void) lut;
	return 0;
}
static int (*const colcpy15_32)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy15_24)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy15_16)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int colcpy15_08(word *dst, byte *src, argb *lut, size_t cnt) {
	if (lut == NULL) {
		lut = defaultPalette;
	}
	for (size_t i = 0; i < cnt; ++i, ++dst, ++src) {
		*dst = toRGB555(lut[*src]);
	}
	return 0;
}
static int (*const colset15_mix)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy15_mix)(void *dst, void *src, void *lut, size_t cnt) = NULL;

// 8 bit bgr color operations
static int colcpy08(void *dst, void *src, void *lut, size_t cnt) {
	memcpy(dst, src, cnt);
	(void) lut;
	return 0;
}
static int colset08(byte *dst, void *src, byte *lut, size_t cnt) {
	for (size_t i = 0; i < cnt; ++i, ++dst) {
		*dst = *lut;
	}
	(void) src;
	return 0;
}
static int colcpy08_bgra(byte *dst, byte *src, void *lut, size_t cnt) {
	// RRRRRRRR`GGGGGGGG`BBBBBBBB => IIIIIIII
	for (size_t i = 0; i < cnt; ++i, ++dst, src += 4) {
		*dst = lum((src[0] << 16) | (src[1] << 8) | src[2]);
	}
	(void) lut;
	return 0;
}
static int colcpy08_bgr(byte *dst, byte *src, void *lut, size_t cnt) {
	// RRRRRRRR`GGGGGGGG`BBBBBBBB => IIIIIIII
	for (size_t i = 0; i < cnt; ++i, ++dst, src += 3) {
		*dst = lum((src[0] << 16) | (src[1] << 8) | src[2]);
	}
	(void) lut;
	return 0;
}
static int (*const colcpy08_32)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy08_24)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy08_16)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy08_15)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colset08_mix)(void *dst, void *src, void *lut, size_t cnt) = NULL;
static int (*const colcpy08_mix)(void *dst, void *src, void *lut, size_t cnt) = NULL;

bltProc getBltProc(BltType type, int depth) {
	switch (type) {
		case blt_cvt_bgra:
			switch (depth) {
				case blt_cvt_32:
					return (bltProc) colcpy32_bgra;
				case blt_cvt_24:
					return (bltProc) colcpy24_bgra;
				case blt_cvt_16:
					return (bltProc) colcpy16_bgra;
				case blt_cvt_15:
					return (bltProc) colcpy15_bgra;
				case blt_cvt_08:
					return (bltProc) colcpy08_bgra;
			}
			break;

		case blt_cvt_bgr:
			switch (depth) {
				case blt_cvt_32:
					return (bltProc) colcpy32_bgr;
				case blt_cvt_24:
					return (bltProc) colcpy24_bgr;
				case blt_cvt_16:
					return (bltProc) colcpy16_bgr;
				case blt_cvt_15:
					return (bltProc) colcpy15_bgr;
				case blt_cvt_08:
					return (bltProc) colcpy08_bgr;
			}
			break;

		case blt_cvt_32:
			switch (depth) {
				case blt_cvt_32:
					return (bltProc) colcpy32;
				case blt_cvt_24:
					return (bltProc) colcpy24_32;
				case blt_cvt_16:
					return (bltProc) colcpy16_32;
				case blt_cvt_15:
					return (bltProc) colcpy15_32;
				case blt_cvt_08:
					return (bltProc) colcpy08_32;
			}
			break;

		case blt_cvt_24:
			switch (depth) {
				case blt_cvt_32:
					return (bltProc) colcpy32_24;
				case blt_cvt_24:
					return (bltProc) colcpy24;
				case blt_cvt_16:
					return (bltProc) colcpy16_24;
				case blt_cvt_15:
					return (bltProc) colcpy15_24;
				case blt_cvt_08:
					return (bltProc) colcpy08_24;
			}
			break;

		case blt_cvt_16:
			switch (depth) {
				case blt_cvt_32:
					return (bltProc) colcpy32_16;
				case blt_cvt_24:
					return (bltProc) colcpy24_16;
				case blt_cvt_16:
					return (bltProc) colcpy16;
				case blt_cvt_15:
					return (bltProc) colcpy15_16;
				case blt_cvt_08:
					return (bltProc) colcpy08_16;
			}
			break;

		case blt_cvt_15:
			switch (depth) {
				case blt_cvt_32:
					return (bltProc) colcpy32_15;
				case blt_cvt_24:
					return (bltProc) colcpy24_15;
				case blt_cvt_16:
					return (bltProc) colcpy16_15;
				case blt_cvt_15:
					return (bltProc) colcpy15;
				case blt_cvt_08:
					return (bltProc) colcpy08_15;
			}
			break;

		case blt_cvt_08:
			switch (depth) {
				case blt_cvt_32:
					return (bltProc) colcpy32_08;
				case blt_cvt_24:
					return (bltProc) colcpy24_08;
				case blt_cvt_16:
					return (bltProc) colcpy16_08;
				case blt_cvt_15:
					return (bltProc) colcpy15_08;
				case blt_cvt_08:
					return (bltProc) colcpy08;
			}
			break;

		case blt_cpy_mix:
			switch (depth) {
				case blt_cvt_32:
					return (bltProc) colcpy32_mix;
				case blt_cvt_24:
					return (bltProc) colcpy24_mix;
				case blt_cvt_16:
					return (bltProc) colcpy16_mix;
				case blt_cvt_15:
					return (bltProc) colcpy15_mix;
				case blt_cvt_08:
					return (bltProc) colcpy08_mix;
			}
			break;

		case blt_set_mix:
			switch (depth) {
				case blt_cvt_32:
					return (bltProc) colset32_mix;
				case blt_cvt_24:
					return (bltProc) colset24_mix;
				case blt_cvt_16:
					return (bltProc) colset16_mix;
				case blt_cvt_15:
					return (bltProc) colset15_mix;
				case blt_cvt_08:
					return (bltProc) colset08_mix;
			}
			break;

		case blt_set_col:
			switch (depth) {
				case blt_cvt_32:
					return (bltProc) colset32;
				case blt_cvt_24:
					return (bltProc) colset24;
				case blt_cvt_16:
					return (bltProc) colset16;
				case blt_cvt_15:
					return (bltProc) colset15;
				case blt_cvt_08:
					return (bltProc) colset08;
			}
			break;
	}
	return NULL;
}

static argb defaultPalette[256] = {
	{0x000000},
	{0x010101},
	{0x020202},
	{0x030303},
	{0x040404},
	{0x050505},
	{0x060606},
	{0x070707},
	{0x080808},
	{0x090909},
	{0x0a0a0a},
	{0x0b0b0b},
	{0x0c0c0c},
	{0x0d0d0d},
	{0x0e0e0e},
	{0x0f0f0f},
	{0x101010},
	{0x111111},
	{0x121212},
	{0x131313},
	{0x141414},
	{0x151515},
	{0x161616},
	{0x171717},
	{0x181818},
	{0x191919},
	{0x1a1a1a},
	{0x1b1b1b},
	{0x1c1c1c},
	{0x1d1d1d},
	{0x1e1e1e},
	{0x1f1f1f},
	{0x202020},
	{0x212121},
	{0x222222},
	{0x232323},
	{0x242424},
	{0x252525},
	{0x262626},
	{0x272727},
	{0x282828},
	{0x292929},
	{0x2a2a2a},
	{0x2b2b2b},
	{0x2c2c2c},
	{0x2d2d2d},
	{0x2e2e2e},
	{0x2f2f2f},
	{0x303030},
	{0x313131},
	{0x323232},
	{0x333333},
	{0x343434},
	{0x353535},
	{0x363636},
	{0x373737},
	{0x383838},
	{0x393939},
	{0x3a3a3a},
	{0x3b3b3b},
	{0x3c3c3c},
	{0x3d3d3d},
	{0x3e3e3e},
	{0x3f3f3f},
	{0x404040},
	{0x414141},
	{0x424242},
	{0x434343},
	{0x444444},
	{0x454545},
	{0x464646},
	{0x474747},
	{0x484848},
	{0x494949},
	{0x4a4a4a},
	{0x4b4b4b},
	{0x4c4c4c},
	{0x4d4d4d},
	{0x4e4e4e},
	{0x4f4f4f},
	{0x505050},
	{0x515151},
	{0x525252},
	{0x535353},
	{0x545454},
	{0x555555},
	{0x565656},
	{0x575757},
	{0x585858},
	{0x595959},
	{0x5a5a5a},
	{0x5b5b5b},
	{0x5c5c5c},
	{0x5d5d5d},
	{0x5e5e5e},
	{0x5f5f5f},
	{0x606060},
	{0x616161},
	{0x626262},
	{0x636363},
	{0x646464},
	{0x656565},
	{0x666666},
	{0x676767},
	{0x686868},
	{0x696969},
	{0x6a6a6a},
	{0x6b6b6b},
	{0x6c6c6c},
	{0x6d6d6d},
	{0x6e6e6e},
	{0x6f6f6f},
	{0x707070},
	{0x717171},
	{0x727272},
	{0x737373},
	{0x747474},
	{0x757575},
	{0x767676},
	{0x777777},
	{0x787878},
	{0x797979},
	{0x7a7a7a},
	{0x7b7b7b},
	{0x7c7c7c},
	{0x7d7d7d},
	{0x7e7e7e},
	{0x7f7f7f},
	{0x808080},
	{0x818181},
	{0x828282},
	{0x838383},
	{0x848484},
	{0x858585},
	{0x868686},
	{0x878787},
	{0x888888},
	{0x898989},
	{0x8a8a8a},
	{0x8b8b8b},
	{0x8c8c8c},
	{0x8d8d8d},
	{0x8e8e8e},
	{0x8f8f8f},
	{0x909090},
	{0x919191},
	{0x929292},
	{0x939393},
	{0x949494},
	{0x959595},
	{0x969696},
	{0x979797},
	{0x989898},
	{0x999999},
	{0x9a9a9a},
	{0x9b9b9b},
	{0x9c9c9c},
	{0x9d9d9d},
	{0x9e9e9e},
	{0x9f9f9f},
	{0xa0a0a0},
	{0xa1a1a1},
	{0xa2a2a2},
	{0xa3a3a3},
	{0xa4a4a4},
	{0xa5a5a5},
	{0xa6a6a6},
	{0xa7a7a7},
	{0xa8a8a8},
	{0xa9a9a9},
	{0xaaaaaa},
	{0xababab},
	{0xacacac},
	{0xadadad},
	{0xaeaeae},
	{0xafafaf},
	{0xb0b0b0},
	{0xb1b1b1},
	{0xb2b2b2},
	{0xb3b3b3},
	{0xb4b4b4},
	{0xb5b5b5},
	{0xb6b6b6},
	{0xb7b7b7},
	{0xb8b8b8},
	{0xb9b9b9},
	{0xbababa},
	{0xbbbbbb},
	{0xbcbcbc},
	{0xbdbdbd},
	{0xbebebe},
	{0xbfbfbf},
	{0xc0c0c0},
	{0xc1c1c1},
	{0xc2c2c2},
	{0xc3c3c3},
	{0xc4c4c4},
	{0xc5c5c5},
	{0xc6c6c6},
	{0xc7c7c7},
	{0xc8c8c8},
	{0xc9c9c9},
	{0xcacaca},
	{0xcbcbcb},
	{0xcccccc},
	{0xcdcdcd},
	{0xcecece},
	{0xcfcfcf},
	{0xd0d0d0},
	{0xd1d1d1},
	{0xd2d2d2},
	{0xd3d3d3},
	{0xd4d4d4},
	{0xd5d5d5},
	{0xd6d6d6},
	{0xd7d7d7},
	{0xd8d8d8},
	{0xd9d9d9},
	{0xdadada},
	{0xdbdbdb},
	{0xdcdcdc},
	{0xdddddd},
	{0xdedede},
	{0xdfdfdf},
	{0xe0e0e0},
	{0xe1e1e1},
	{0xe2e2e2},
	{0xe3e3e3},
	{0xe4e4e4},
	{0xe5e5e5},
	{0xe6e6e6},
	{0xe7e7e7},
	{0xe8e8e8},
	{0xe9e9e9},
	{0xeaeaea},
	{0xebebeb},
	{0xececec},
	{0xededed},
	{0xeeeeee},
	{0xefefef},
	{0xf0f0f0},
	{0xf1f1f1},
	{0xf2f2f2},
	{0xf3f3f3},
	{0xf4f4f4},
	{0xf5f5f5},
	{0xf6f6f6},
	{0xf7f7f7},
	{0xf8f8f8},
	{0xf9f9f9},
	{0xfafafa},
	{0xfbfbfb},
	{0xfcfcfc},
	{0xfdfdfd},
	{0xfefefe},
	{0xffffff}
};
// TODO or NOT TODO
//static int colset32_and(argb *dst, void *src, argb *lut, size_t cnt);
//static int colset32_ior(argb *dst, void *src, argb *lut, size_t cnt);
//static int colset32_xor(argb *dst, void *src, argb *lut, size_t cnt);
//static int colset32_add(argb *dst, void *src, argb *lut, size_t cnt);
//static int colset32_sub(argb *dst, void *src, argb *lut, size_t cnt);
//static int colset32_mul(argb *dst, void *src, argb *lut, size_t cnt);
//static int colset32_div(argb *dst, void *src, argb *lut, size_t cnt);
//static int colset32_min(argb *dst, void *src, argb *lut, size_t cnt);
//static int colset32_max(argb *dst, void *src, argb *lut, size_t cnt);
//static int colcpy32_and(argb *dst, void *src, argb *lut, size_t cnt);
//static int colcpy32_ior(argb *dst, void *src, argb *lut, size_t cnt);
//static int colcpy32_xor(argb *dst, void *src, argb *lut, size_t cnt);
//static int colcpy32_add(argb *dst, void *src, argb *lut, size_t cnt);
//static int colcpy32_sub(argb *dst, void *src, argb *lut, size_t cnt);
//static int colcpy32_mul(argb *dst, void *src, argb *lut, size_t cnt);
//static int colcpy32_div(argb *dst, void *src, argb *lut, size_t cnt);
//static int colcpy32_min(argb *dst, void *src, argb *lut, size_t cnt);
//static int colcpy32_max(argb *dst, void *src, argb *lut, size_t cnt);
// copy from src to dst using a lookup table
//static int colcpy32_lut(argb *dst, void *src, argb *lut, size_t cnt);
// copy from src to dst using a color matrix
//static int colcpy32_mat(argb *dst, void *src, argb *lut, size_t cnt);
// copy from src to dst using a polynomial
//static int colcpy32_pol(argb *dst, void *src, argb *lut, size_t cnt);
