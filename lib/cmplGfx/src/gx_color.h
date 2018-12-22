#ifndef __GX_COLOR
#define __GX_COLOR

#include <stdint.h>

typedef union {				// ARGB color structutre
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
	cblt_conv_32 = 32,
	cblt_conv_24 = 24,
	cblt_conv_16 = 16,
	cblt_conv_15 = 15,
	cblt_conv_08 = 8,
	//cblt_conv_04 = 4,
	//cblt_conv_02 = 2,
	//cblt_conv_01 = 1,

	cblt_cpy_mix = 17,
	cblt_cpy_and = 18,
	cblt_cpy_ior = 19,
	cblt_cpy_xor = 20,

	cblt_set_mix = 25,
	//cblt_set_and = 26,
	//cblt_set_ior = 27,
	//cblt_set_xor = 28,
} cblt_type;

typedef void (*cblt_proc)(void* dst, void *src, void *lut, size_t cnt);


static inline uint32_t bch(uint32_t xrgb) { return xrgb >>  0 & 0xff; }
static inline uint32_t gch(uint32_t xrgb) { return xrgb >>  8 & 0xff; }
static inline uint32_t rch(uint32_t xrgb) { return xrgb >> 16 & 0xff; }
static inline uint32_t ach(uint32_t xrgb) { return xrgb >> 24 & 0xff; }

static inline uint32_t lum(argb col) {
	return (uint32_t) ((col.r * 76 + col.g * 150 + col.b * 29) >> 8);
}

static inline uint8_t clamp_s8(int32_t val) {
	if (val > 255) {
		val = 255;
	}
	if (val < 0) {
		val = 0;
	}
	return (uint8_t) val;
}
static inline uint8_t clamp_u8(uint32_t val) {
	if (val > 255) {
		val = 255;
	}
	return (uint8_t) val;
}

static inline argb make_rgb(unsigned a, unsigned r, unsigned g, unsigned b) {
	return (argb)(uint32_t)(a << 24 | r << 16 | g << 8 | b);
}
static inline argb clamp_srgb(signed a, signed r, signed g, signed b) {
	return make_rgb(clamp_s8(a), clamp_s8(r), clamp_s8(g), clamp_s8(b));
}
static inline argb clamp_urgb(unsigned a, unsigned r, unsigned g, unsigned b) {
	return make_rgb(clamp_u8(a), clamp_u8(r), clamp_u8(g), clamp_u8(b));
}


static inline argb gx_mixcolor(argb c1, argb c2, int alpha) {
	//~ uint32_t r = (c1 & 0xff0000) + (alpha * ((c2 & 0xff0000) - (c1 & 0xff0000)) >> 8);
	//~ uint32_t g = (c1 & 0xff00) + (alpha * ((c2 & 0xff00) - (c1 & 0xff00)) >> 8);
	//~ uint32_t b = (c1 & 0xff) + (alpha * ((c2 & 0xff) - (c1 & 0xff)) >> 8);
	//~ return (r & 0xff0000) | (g & 0xff00) | (b & 0xff);

	uint32_t rb = c1.val & 0xff00ff;
	rb += alpha * ((c2.val & 0xff00ff) - rb) >> 8;

	uint32_t g = c1.val & 0x00ff00;
	g += alpha * ((c2.val & 0x00ff00) - g) >> 8;

	return (argb)((rb & 0xff00ff) | (g & 0x00ff00));
}
static inline uint8_t gx_mixgray(uint32_t c1, uint32_t c2, int alpha) {
	uint32_t c = c1 & 0xff;
	c += alpha * ((c2 & 0xff) - c) >> 8;
	return (uint8_t) (c & 0xff);
}


void colcpy_32_abgr(void* dst, void *src, void *lut, size_t cnt);
void colcpy_32_bgr(void* dst, void *src, void *lut, size_t cnt);
void colcpy_32_08(void* dst, void *src, void *lut, size_t cnt);

void colset_32mix(void* dst, void *src, void *lut, size_t cnt);

cblt_proc gx_getcbltf(cblt_type type, int srcDepth);

#endif
