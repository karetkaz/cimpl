#ifndef __GX_COLOR
#define __GX_COLOR

#include <stdint.h>
#include <stddef.h>

typedef union {				// ARGB color structure
	uint32_t val;
	struct {
		uint8_t b;
		uint8_t g;
		uint8_t r;
		uint8_t a;
	};
	/*struct {
		uint8_t b;
		uint8_t g;
		uint8_t r;
		uint8_t a;
	} rgb;
	struct {
		uint8_t k;
		uint8_t y;
		uint8_t m;
		uint8_t c;
	} cmy;
	struct {
		uint8_t  l;
		uint8_t  s;
		uint16_t h;
	} hsl;
	struct {
		uint8_t  v;
		uint8_t  s;
		uint16_t h;
	} hsv;
	struct {
		uint8_t c1;
		uint8_t c2;
		uint16_t y;
	} yiq;
	struct {
		uint8_t u;
		uint8_t v;
		uint16_t y;
	} yuv;
	enum {
		cs_argb = 'argb',
		cs_cmyk = 'cmyk',
		cs_hsv  = '_hsv',
		cs_yuv  = '_yuv',
		//~ cs_xyz  = ' xyz',
		//~ cs_rgb  = ' rgb',
		//~ cs_cmy  = ' cmy',
		//~ cs_hsl  = ' hsl',
		//~ cs_yiq  = ' yiq',
	} argb_cs; // */
} argb;

typedef enum {			// color block transfer
	blt_cvt_32 = 32,
	blt_cvt_24 = 24,
	blt_cvt_16 = 16,
	blt_cvt_15 = 15,
	blt_cvt_08 = 8,
	//blt_cvt_04 = 4,
	//blt_cvt_02 = 2,
	//blt_cvt_01 = 1,

	// 24 - 32 custom conversions methods
	blt_cvt_bgra = 31,
	blt_cvt_bgr = 30,
	// to add: hsv, yuv, 16bit gray, 16bit rgb, r10g10b10

	blt_cpy_mix = 20,
	blt_set_mix = 19,
	blt_set_col = 18,
} BltType;

typedef int (*bltProc)(void* dst, void *src, void *lut, size_t cnt);

static inline int32_t bch(uint32_t color) { return color >>  0 & 0xff; }
static inline int32_t gch(uint32_t color) { return color >>  8 & 0xff; }
static inline int32_t rch(uint32_t color) { return color >> 16 & 0xff; }
static inline int32_t ach(uint32_t color) { return color >> 24 & 0xff; }

static inline int32_t hue(uint32_t color) {
	// adapted from: https://gist.github.com/mity/6034000
	int r = rch(color);
	int g = gch(color);
	int b = bch(color);
	int min = r;
	int max = r;

	if (min > g) {
		min = g;
	}
	if (min > b) {
		min = b;
	}
	if (max < g) {
		max = g;
	}
	if (max < b) {
		max = b;
	}

	if (max == min) {
		return 0;
	}

	int hue = 0;
	if (max == r) {
		hue = ((g - b) * 60) / (max - min);
	}
	else if (max == g) {
		hue = ((b - r) * 60) / (max - min) + 120;
	}
	else if (max == b) {
		hue = ((r - g) * 60) / (max - min) + 240;
	}
	if (hue < 0) {
		hue += 360;
	}
	return hue;
}
static inline int32_t lum(uint32_t color) {
	// based on formula: 0.299 * color.r + 0.587 * color.g + 0.114 * color.b;
	return (19595 * rch(color) + 38470 * gch(color) + 7471 * bch(color)) >> 16;
}

static inline uint8_t sat_s8(int32_t val) {
	if (val > 255) {
		val = 255;
	}
	if (val < 0) {
		val = 0;
	}
	return (uint8_t) val;
}
static inline uint8_t sat_u8(uint32_t val) {
	if (val > 255) {
		val = 255;
	}
	return (uint8_t) val;
}

static inline argb cast_rgb(uint32_t val) {
	return *(argb *) &val;
}

static inline argb make_rgb(unsigned a, unsigned r, unsigned g, unsigned b) {
	return cast_rgb(a << 24 | r << 16 | g << 8 | b);
}
static inline argb sat_srgb(signed a, signed r, signed g, signed b) {
	return make_rgb(sat_s8(a), sat_s8(r), sat_s8(g), sat_s8(b));
}
static inline argb sat_urgb(unsigned a, unsigned r, unsigned g, unsigned b) {
	return make_rgb(sat_u8(a), sat_u8(r), sat_u8(g), sat_u8(b));
}


static inline uint8_t mix_s8(int c1, int c2, int alpha) {
	return c1 + (alpha * (c2 - c1 + 1) >> 8);
}
static inline uint8_t mix_u8(int c1, int c2, int alpha) {
	return c1 + (alpha * (c2 - c1 + 1) >> 8);
}
static inline argb mix_rgb(argb c1, argb c2, int alpha) {
	//~ uint32_t r = (c1 & 0xff0000) + (alpha * ((c2 & 0xff0000) - (c1 & 0xff0000)) >> 8);
	//~ uint32_t g = (c1 & 0xff00) + (alpha * ((c2 & 0xff00) - (c1 & 0xff00)) >> 8);
	//~ uint32_t b = (c1 & 0xff) + (alpha * ((c2 & 0xff) - (c1 & 0xff)) >> 8);
	//~ return (r & 0xff0000) | (g & 0xff00) | (b & 0xff);

	uint32_t rb = c1.val & 0xff00ff;
	rb += alpha * ((c2.val & 0xff00ff) - rb) >> 8;

	uint32_t g = c1.val & 0x00ff00;
	g += alpha * ((c2.val & 0x00ff00) - g) >> 8;

	return cast_rgb((rb & 0xff00ff) | (g & 0x00ff00));
}

bltProc getBltProc(BltType type, int depth);

#endif
