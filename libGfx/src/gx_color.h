#ifndef __GX_COLOR
#define __GX_COLOR

#include <stdint.h>

typedef union {				// ARGB color structutre
	uint32_t val;
	uint32_t col;
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


static inline uint32_t rch(uint32_t xrgb) { return xrgb >> 16 & 0xff; }
static inline uint32_t gch(uint32_t xrgb) { return xrgb >>  8 & 0xff; }
static inline uint32_t bch(uint32_t xrgb) { return xrgb >>  0 & 0xff; }

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

static inline uint32_t rgblum(argb col) {
	return (uint32_t) ((col.r * 76 + col.g * 150 + col.b * 29) >> 8);
}

static inline argb rgbval(uint32_t val) {
	return (argb) val;
}
static inline argb rgbrgb(int r, int g, int b) {
	argb res;
	//~ res.a = 0xff;
	res.r = (uint8_t) (r & 0xff);
	res.g = (uint8_t) (g & 0xff);
	res.b = (uint8_t) (b & 0xff);
	return res;
}


static inline uint8_t clamp_val(int val) {
	if (val > 255) {
		val = 255;
	}
	if (val < 0) {
		val = 0;
	}
	return (uint8_t) val;
}
static inline argb clamp_rgb(int r, int g, int b) {
	argb res;
	//~ res.a = 0xff;
	res.r = clamp_val(r);
	res.g = clamp_val(g);
	res.b = clamp_val(b);
	return res;
}

static inline argb rgbset(int a, int r, int g, int b) {
	argb res;
	res.a = (uint8_t) (a & 0xff);
	res.r = (uint8_t) (r & 0xff);
	res.g = (uint8_t) (g & 0xff);
	res.b = (uint8_t) (b & 0xff);
	return res;
}
static inline uint32_t __rgb(int r, int g, int b) { return (uint32_t) (r << 16 | g << 8 | b); }
static inline uint32_t __argb(int a, int r, int g, int b) { return (uint32_t) (a << 24 | r << 16 | g << 8 | b); }

void colcpy_32_abgr(void* dst, void *src, void *lut, size_t cnt);
void colcpy_32_bgr(void* dst, void *src, void *lut, size_t cnt);
void colcpy_32_08(void* dst, void *src, void *lut, size_t cnt);

void colset_32mix(void* dst, void *src, void *lut, size_t cnt);

cblt_proc gx_getcbltf(cblt_type type, int srcDepth);

#endif
