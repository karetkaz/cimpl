#ifndef __GX_SURFACE
#define __GX_SURFACE

#include <stdio.h>
#include <stdint.h>
#include "gx_color.h"

typedef enum {
	SurfType = 0x000f,		// 0-15
	Surf_2ds = 0x0000,		// 2d surface (tempPtr:= null)
	Surf_pal = 0x0001,		// indexed (tempPtr: CLUTPtr: palette)
	Surf_fnt = 0x0002,		// font (tempPtr: LLUTPtr: fontface)
	Surf_3ds = 0x0003,		// 3d surface (tempPtr: zbuff)
	Surf_freeData = 0x0100,		// delete basePtr on destroy
	Surf_freeSurf = 0x0200,		// delete reference on destroy
	//Surf_recycle  = 0x0400,
} surfFlags;

typedef enum {
	gradient_lin = 0x00,		// Linear
	gradient_rad = 0x01,		// Radial (elliptical)
	gradient_sqr = 0x02,		// Square
	gradient_con = 0x03,		// Conical
	gradient_spr = 0x04,		// Spiral
	mask_type    = 0x07,		// gradient type
	flag_repeat  = 0x08,		// repeat
	flag_alpha   = 0x10,		// write alpha channel only
} gradient_type;

// clipping bound structure [bounding box]
typedef struct gx_Clip {
	union { uint16_t l, xmin; };	//(left)
	union { uint16_t t, ymin; };	//(top)
	union { uint16_t r, xmax; };	//(right)
	union { uint16_t b, ymax; };	//(bottom)
} *gx_Clip;

// Rectangle
typedef struct gx_Rect {
	int32_t x;
	int32_t y;
	union { int32_t w, width; };
	union { int32_t h, height; };
} *gx_Rect;

// Color Look Up Table (Palette)
typedef struct gx_Clut {
	uint16_t        count;
	uint8_t         flags;
	uint8_t         trans;
	argb            data[256];
} *gx_Clut;

// Layer Look Up Table
typedef struct gx_Llut {
	uint16_t        count;			// font faces
	uint16_t        height;			// font height
	struct gx_FDIR {
		uint8_t     pad_x;
		uint8_t     pad_y;
		uint16_t    width;
		void*       basePtr;
	} data[256];
} *gx_Llut;

// surface structure
typedef struct gx_Surf {
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

	gx_Clip clipPtr;

	union {
		gx_Clut CLUTPtr;	// palette
		gx_Llut LLUTPtr;	// font-faces
		// gx_Llut facePtr;
		char *tempPtr; // clutPtr / cibgPtr (cursor BG data) / ...
	};
} *gx_Surf;

#ifdef __cplusplus
extern "C" {
#endif

gx_Surf gx_createSurf(gx_Surf recycle, int width, int height, int depth, surfFlags flags);
void gx_destroySurf(gx_Surf surf);

// Get current clip region
static inline gx_Clip gx_getclip(gx_Surf surf) {
	return surf->clipPtr ? surf->clipPtr : (gx_Clip)surf;
}
void* gx_cliprect(gx_Surf surf, gx_Rect rect);

// Get Pixel Address
static inline void* gx_getpaddr(gx_Surf surf, int x, int y) {
	if ((unsigned) x >= surf->width || (unsigned) y >= surf->height) {
		return NULL;
	}
	return surf->basePtr + ((size_t) y * surf->scanLen) + ((size_t) x * surf->pixeLen);
}

// Get Pixel Color
static inline uint32_t gx_getpixel(gx_Surf surf, int x, int y) {
	void *address = gx_getpaddr(surf, x, y);
	if (address != NULL) {
		switch (surf->depth) {
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
static inline void gx_setpixel(gx_Surf surf, int x, int y, uint32_t color) {
	void *address = gx_getpaddr(surf, x, y);
	if (address != NULL) {
		switch (surf->depth) {
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
// Get Pixel Color linear
static inline uint32_t gx_getpix16(gx_Surf surf, int32_t x16, int32_t y16) {
	int32_t lx = x16 >> 8 & 0xff;
	int32_t ly = y16 >> 8 & 0xff;
	int32_t x = x16 >> 16;
	int32_t y = y16 >> 16;

	char *ofs = gx_getpaddr(surf, x, y);
	if (ofs == NULL) {
		// no pixel, return black
		return 0;
	}

	if (x + 1 >= surf->width) {
		// no interpolation, no next pixel
		lx = 0;
	}
	if (y + 1 >= surf->height) {
		// no interpolation, no next pixel
		ly = 0;
	}

	switch (surf->depth) {
		case 32:
			if (lx && ly) {
				argb p0 = *(argb*)(ofs + 0);
				argb p1 = *(argb*)(ofs + 4);
				ofs += surf->scanLen;
				argb p2 = *(argb*)(ofs + 0);
				argb p3 = *(argb*)(ofs + 4);

				p0 = gx_mixcolor(p0, p1, lx);
				p2 = gx_mixcolor(p2, p3, lx);
				return gx_mixcolor(p0, p2, ly).val;
			}
			if (lx) {
				argb p0 = *(argb*)(ofs + 0);
				argb p1 = *(argb*)(ofs + 4);
				return gx_mixcolor(p0, p1, lx).val;
			}
			if (ly) {
				argb p0 = *(argb*)(ofs + 0);
				argb p1 = *(argb*)(ofs + surf->scanLen);
				return gx_mixcolor(p0, p1, ly).val;
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
				ofs += surf->scanLen;
				uint8_t p2 = *(uint8_t*)(ofs + 0);
				uint8_t p3 = *(uint8_t*)(ofs + 1);

				p0 = gx_mixgray(p0, p1, lx);
				p2 = gx_mixgray(p2, p3, lx);
				return gx_mixgray(p0, p2, ly);
			}
			if (lx) {
				uint8_t p0 = *(uint8_t*)(ofs + 0);
				uint8_t p1 = *(uint8_t*)(ofs + 1);
				return gx_mixgray(p0, p1, lx);
			}
			if (ly) {
				uint8_t p0 = *(uint8_t*)(ofs + 0);
				uint8_t p1 = *(uint8_t*)(ofs + surf->scanLen);
				return gx_mixgray(p0, p1, ly);
			}
			return *(uint8_t*)ofs;
	}
	return 0;
}


// draw
void g2_drawRect(gx_Surf surf, int x1, int y1, int x2, int y2, uint32_t color);
void g2_fillRect(gx_Surf surf, int x1, int y1, int x2, int y2, uint32_t color);

void g2_drawOval(gx_Surf surf, int x1, int y1, int x2, int y2, uint32_t color);
void g2_fillOval(gx_Surf surf, int x1, int y1, int x2, int y2, uint32_t color);

void g2_drawLine(gx_Surf surf, int x1, int y1, int x2, int y2, uint32_t color);
void g2_drawBez2(gx_Surf surf, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
void g2_drawBez3(gx_Surf surf, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, uint32_t color);

void g2_clipText(gx_Rect rect, gx_Surf font, const char *text);
void g2_drawChar(gx_Surf surf, int x, int y, gx_Surf font, int chr, uint32_t color);
void g2_drawText(gx_Surf surf, int x, int y, gx_Surf font, const char *text, uint32_t color);
static inline void g2_drawTextRoi(gx_Surf surf, gx_Rect rect, gx_Surf font, const char *text, uint32_t color) {
	struct gx_Rect textRect = *rect;
	if (!gx_cliprect(surf, &textRect)) {
		return;
	}

	struct gx_Clip textClip;
	textClip.t = textRect.y;
	textClip.l = textRect.x;
	textClip.r = textRect.x + textRect.w;
	textClip.b = textRect.y + textRect.h;
	gx_Clip oldClip = surf->clipPtr;
	surf->clipPtr = &textClip;
	g2_drawText(surf, rect->x, rect->y, font, text, color);
	surf->clipPtr = oldClip;
}

int gx_blitSurf(gx_Surf surf, int x, int y, gx_Surf src, gx_Rect roi, void *extra, cblt_proc blt);
int gx_zoomSurf(gx_Surf surf, gx_Rect rect, gx_Surf src, gx_Rect roi, int interpolate);
int gx_blurSurf(gx_Surf surf, int radius, double sigma);
static inline int gx_copySurf(gx_Surf surf, int x, int y, gx_Surf src, gx_Rect roi) {
	return gx_blitSurf(surf, x, y, src, roi, NULL, gx_getcbltf(surf->depth, src->depth));
}
static inline int gx_lerpSurf(gx_Surf surf, int x, int y, gx_Surf src, gx_Rect roi, int alpha) {
	if (surf->depth != src->depth) {
		// source and destination must have same depth
		return -2;
	}
	return gx_blitSurf(surf, x, y, src, roi, (void *) (ssize_t) alpha, gx_getcbltf(cblt_cpy_mix, src->depth));
}


// image read write
gx_Surf gx_loadBmp(gx_Surf dst, const char *src, int depth);
int gx_saveBmp(const char *dst, gx_Surf src, int flags);

gx_Surf gx_loadFnt(gx_Surf dst, const char *src);
gx_Surf gx_loadJpg(gx_Surf dst, const char *src, int depth);
gx_Surf gx_loadPng(gx_Surf dst, const char *src, int depth);


// image effects
int gx_gradSurf(gx_Surf dst, gx_Rect roi, gradient_type type, int length, uint32_t colors[]);

#if __cplusplus
}
#endif

#define LOBIT(__VAL) ((__VAL) & -(__VAL))
#define gx_debug(msg, ...) do { fprintf(stdout, "%s:%d: %s: debug: "msg"\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)

#endif
