#ifndef __GX_SURFACE
#define __GX_SURFACE

#include <stdio.h>
#include <stdint.h>
#include "gx_color.h"

typedef enum {
	ImageType = 0x000f,		// 0-15
	Image2d = 0x0000,		// 2d image (tempPtr:= null)
	ImageIdx = 0x0001,		// indexed (tempPtr: CLUTPtr: palette)
	ImageFnt = 0x0002,		// font (tempPtr: LLUTPtr: fontface)
	Image3d = 0x0003,		// 3d image (tempPtr: zbuff)
	freeData = 0x0100,		// delete basePtr on destroy
	freeImage = 0x0200,		// delete reference on destroy
} ImageFlags;

typedef enum {
	gradient_lin = 0x00,		// Linear
	gradient_rad = 0x01,		// Radial (elliptical)
	gradient_sqr = 0x02,		// Square
	gradient_con = 0x03,		// Conical
	gradient_spr = 0x04,		// Spiral
	mask_type    = 0x07,		// gradient type
	flag_repeat  = 0x08,		// repeat
	flag_alpha   = 0x10,		// write alpha channel only
} GradientFlags;

// bounding box
typedef struct GxClip {
	union { uint16_t l, xmin; };	//(left)
	union { uint16_t t, ymin; };	//(top)
	union { uint16_t r, xmax; };	//(right)
	union { uint16_t b, ymax; };	//(bottom)
} *GxClip;

// Rectangle
typedef struct GxRect {
	int32_t x;
	int32_t y;
	union { int32_t w, width; };
	union { int32_t h, height; };
} *GxRect;

// Color Look Up Table (Palette)
typedef struct GxCLut {
	uint16_t count;
	uint8_t flags;
	uint8_t trans;
	argb data[256];
} *GxCLut;

// Layer Look Up Table
typedef struct GxFLut {
	uint32_t count;            // font faces
	struct GxFace {
		uint8_t pad_x;
		uint8_t pad_y;
		uint16_t width;
		void *basePtr;
	} data[256];
} *GxFLut;

// image structure
typedef struct GxImage {
	struct {
		uint16_t x0;
		uint16_t y0;
	};
	uint16_t width;
	uint16_t height;

	uint16_t flags;
	uint8_t depth;
	uint8_t pixeLen;
	uint32_t scanLen;

	char *basePtr;

	GxClip clipPtr;

	union {
		GxCLut CLUTPtr;	// palette
		GxFLut LLUTPtr;	// font-faces
		char *tempPtr; // clutPtr / cibgPtr (cursor BG data) / ...
	};
} *GxImage;

#ifdef __cplusplus
extern "C" {
#endif

GxImage createImage(GxImage recycle, int width, int height, int depth, ImageFlags flags);
GxImage sliceImage(GxImage recycle, GxImage parent, GxRect roi);
void destroyImage(GxImage image);

// Get current clip region
static inline GxClip getClip(GxImage image) {
	return image->clipPtr ? image->clipPtr : (GxClip)image;
}
void* clipRect(GxImage image, GxRect roi);

// Get Pixel Address
static inline void* getPAddr(GxImage image, int x, int y) {
	if ((unsigned) x >= image->width || (unsigned) y >= image->height) {
		return NULL;
	}
	return image->basePtr + ((size_t) y * image->scanLen) + ((size_t) x * image->pixeLen);
}

// Get Pixel Color
static inline uint32_t getPixel(GxImage image, int x, int y) {
	void *address = getPAddr(image, x, y);
	if (address != NULL) {
		switch (image->depth) {
			case 32:
			case 24:
				return *(uint32_t*)address;
			case 15:
			case 16:
				return *(uint16_t*)address;
			case 8:
				return *(uint8_t*)address;
		}
	}
	return 0;
}
// Set Pixel Color
static inline void setPixel(GxImage image, int x, int y, uint32_t color) {
	void *address = getPAddr(image, x, y);
	if (address != NULL) {
		switch (image->depth) {
			case 32:
				// TODO: case 24:
				*(uint32_t*)address = color;
				break;

			case 16:
			case 15:
				*(uint16_t*)address = color;
				break;

			case 8:
				*(uint8_t*)address = color;
				break;
		}
	}
}
// Get Pixel Color using linear interpolation
static inline uint32_t getPixelLinear(GxImage image, int32_t x16, int32_t y16) {
	int32_t lx = x16 >> 8 & 0xff;
	int32_t ly = y16 >> 8 & 0xff;
	int32_t x = x16 >> 16;
	int32_t y = y16 >> 16;

	char *ofs = getPAddr(image, x, y);
	if (ofs == NULL) {
		// no pixel, return black
		return 0;
	}

	if (x + 1 >= image->width) {
		// no interpolation, no next pixel
		lx = 0;
	}
	if (y + 1 >= image->height) {
		// no interpolation, no next pixel
		ly = 0;
	}

	switch (image->depth) {
		case 32:
			if (lx && ly) {
				argb p0 = *(argb*)(ofs + 0);
				argb p1 = *(argb*)(ofs + 4);
				ofs += image->scanLen;
				argb p2 = *(argb*)(ofs + 0);
				argb p3 = *(argb*)(ofs + 4);

				p0 = mix_rgb(p0, p1, lx);
				p2 = mix_rgb(p2, p3, lx);
				return mix_rgb(p0, p2, ly).val;
			}
			if (lx) {
				argb p0 = *(argb*)(ofs + 0);
				argb p1 = *(argb*)(ofs + 4);
				return mix_rgb(p0, p1, lx).val;
			}
			if (ly) {
				argb p0 = *(argb*)(ofs + 0);
				argb p1 = *(argb*)(ofs + image->scanLen);
				return mix_rgb(p0, p1, ly).val;
			}
			return *(uint32_t*)ofs;
		case 24:
		case 15:
		case 16:
			// TODO: not implemented
			return *(uint16_t*)ofs;
		case 8:
			if (lx && ly) {
				uint8_t p0 = *(uint8_t*)(ofs + 0);
				uint8_t p1 = *(uint8_t*)(ofs + 1);
				ofs += image->scanLen;
				uint8_t p2 = *(uint8_t*)(ofs + 0);
				uint8_t p3 = *(uint8_t*)(ofs + 1);

				p0 = mix_u8(p0, p1, lx);
				p2 = mix_u8(p2, p3, lx);
				return mix_u8(p0, p2, ly);
			}
			if (lx) {
				uint8_t p0 = *(uint8_t*)(ofs + 0);
				uint8_t p1 = *(uint8_t*)(ofs + 1);
				return mix_u8(p0, p1, lx);
			}
			if (ly) {
				uint8_t p0 = *(uint8_t*)(ofs + 0);
				uint8_t p1 = *(uint8_t*)(ofs + image->scanLen);
				return mix_u8(p0, p1, ly);
			}
			return *(uint8_t*)ofs;
	}
	return 0;
}

// draw
void drawRect(GxImage image, int x1, int y1, int x2, int y2, uint32_t color);
void fillRect(GxImage image, int x1, int y1, int x2, int y2, uint32_t color);

void drawOval(GxImage image, int x1, int y1, int x2, int y2, uint32_t color);
void fillOval(GxImage image, int x1, int y1, int x2, int y2, uint32_t color);

void drawLine(GxImage image, int x1, int y1, int x2, int y2, uint32_t color);
void drawBez2(GxImage image, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
void drawBez3(GxImage image, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, uint32_t color);

void clipText(GxRect rect, GxImage font, const char *text);
void drawChar(GxImage image, int x, int y, GxImage font, int chr, uint32_t color);
void drawText(GxImage image, int x, int y, GxImage font, const char *text, uint32_t color);

int blitImage(GxImage image, int x, int y, GxImage src, GxRect roi, void *extra, bltProc blt);
int resizeImage(GxImage image, GxRect rect, GxImage src, GxRect roi, int interpolation);

// image read write
GxImage loadBmp(GxImage dst, const char *src, int depth);
GxImage loadFnt(GxImage dst, const char *src);
GxImage loadJpg(GxImage dst, const char *src, int depth);
GxImage loadPng(GxImage dst, const char *src, int depth);
int saveBmp(const char *dst, GxImage src, int flags);


// image effects
int blurImage(GxImage image, int radius, double sigma);

int drawGradient(GxImage dst, GxRect roi, GradientFlags type, int length, uint32_t *colors);

static inline int copyImage(GxImage image, int x, int y, GxImage src, GxRect roi) {
	return blitImage(image, x, y, src, roi, NULL, getBltProc(image->depth, src->depth));
}

static inline int blendImage(GxImage image, int x, int y, GxImage src, GxRect roi, int alpha) {
	if (image->depth != src->depth) {
		// source and destination must have same depth
		return -2;
	}
	return blitImage(image, x, y, src, roi, (void *) (ssize_t) alpha, getBltProc(cblt_cpy_mix, src->depth));
}

#if __cplusplus
}
#endif

#define LOBIT(__VAL) ((__VAL) & -(__VAL))
#define gx_debug(msg, ...) do { fprintf(stdout, "%s:%d: %s: debug: "msg"\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)

#endif
