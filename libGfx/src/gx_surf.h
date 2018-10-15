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
	gradient_lin = 0x000000,		// Linear
	gradient_rad = 0x010000,		// Radial (elliptical)
	gradient_sqr = 0x020000,		// Square
	gradient_con = 0x030000,		// Conical
	gradient_spr = 0x040000,		// Spiral
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
	uint32_t        data[256];
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
static inline const gx_Clip gx_getclip(gx_Surf surf) {
	return surf->clipPtr ? surf->clipPtr : (gx_Clip)surf;
}
void* gx_cliprect(gx_Surf surf, gx_Rect rect);

// Get Pixel Address
static inline void* gx_getpaddr(gx_Surf surf, int x, int y) {
	if ((unsigned)x >= surf->width || (unsigned)y >= surf->height) {
		return NULL;
	}
	return surf->basePtr + (y * surf->scanLen) + (x * surf->pixeLen);

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
			//case 24:
				*(uint32_t*)address = color;
			case 16:
			case 15:
				*(uint16_t*)address = color;
			case 8:
				*(uint8_t*)address = color;
		}
	}
}
// Get Pixel Color linear
static inline uint32_t gx_getpix16(gx_Surf surf, int32_t fpx, int32_t fpy, int linear) {
	int32_t x = (uint32_t)fpx >> 16;
	int32_t y = (uint32_t)fpy >> 16;
	char *ofs = gx_getpaddr(surf, x, y);

	if (ofs == NULL) {
		return 0;
	}

	int32_t lx = fpx >> 8 & 0xff;
	int32_t ly = fpy >> 8 & 0xff;
	if (x + 1 >= surf->width) {
		lx = 0;
	}
	if (y + 1 >= surf->height) {
		ly = 0;
	}
	if (!linear) {
		lx = ly = 0;
	}

	if (surf->depth == 32) {
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
		else if (lx) {
			argb p0 = *(argb*)(ofs + 0);
			argb p1 = *(argb*)(ofs + 4);
			return gx_mixcolor(p0, p1, lx).val;
		}
		else if (ly) {
			argb p0 = *(argb*)(ofs + 0);
			argb p1 = *(argb*)(ofs + surf->scanLen);
			return gx_mixcolor(p0, p1, ly).val;
		}
		return *(uint32_t*)ofs;
	}
	else if (surf->depth == 8) {
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
		else if (lx) {
			uint8_t p0 = *(uint8_t*)(ofs + 0);
			uint8_t p1 = *(uint8_t*)(ofs + 1);
			return gx_mixgray(p0, p1, lx);
		}
		else if (ly) {
			uint8_t p0 = *(uint8_t*)(ofs + 0);
			uint8_t p1 = *(uint8_t*)(ofs + surf->scanLen);
			return gx_mixgray(p0, p1, ly);
		}
		return *(uint8_t*)ofs;
	}

	return gx_getpixel(surf, x, y);
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

int gx_copySurf(gx_Surf surf, int x, int y, gx_Surf src, gx_Rect roi);
int gx_zoomSurf(gx_Surf surf, gx_Rect rect, gx_Surf src, gx_Rect roi, int interpolate);


// image read write
gx_Surf gx_loadBmp(gx_Surf dst, const char *src, int depth);
int gx_saveBmp(const char *dst, gx_Surf src, int flags);

gx_Surf gx_loadFnt(gx_Surf dst, const char *src);
gx_Surf gx_loadJpg(gx_Surf dst, const char *src, int depth);
gx_Surf gx_loadPng(gx_Surf dst, const char *src, int depth);


// image effects
int gx_cLutSurf(gx_Surf surf, gx_Rect roi, gx_Clut lut);
int gx_cMatSurf(gx_Surf surf, gx_Rect roi, const int32_t mat[16]);
int gx_gradSurf(gx_Surf dst, gx_Rect roi, gx_Clut lut, gradient_type gradtype, int repeat);

//~ TODO: remove Internal functions

#if __cplusplus
}
#endif

#define LOBIT(__VAL) ((__VAL) & -(__VAL))
#define gx_debug(msg, ...) fprintf(stdout, "%s:%d: debug: "msg"\n", __FILE__, __LINE__, ##__VA_ARGS__)

#endif
