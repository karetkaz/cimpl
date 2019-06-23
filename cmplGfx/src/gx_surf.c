#include "gx_color.h"
#include "gx_surf.h"
#include "g3_math.h"

#include <stdlib.h>

gx_Surf gx_createSurf(gx_Surf recycle, int width, int height, int depth, surfFlags flags) {
	if (recycle == NULL) {
		recycle = (gx_Surf)malloc(sizeof(struct gx_Surf));
		if (recycle == NULL) {
			return NULL;
		}
		flags |= Surf_freeSurf;
		recycle->basePtr = NULL;
		recycle->flags = flags;
	}

	if (recycle->flags & Surf_freeData) {
		free(recycle->basePtr);
	}

	recycle->x0 = recycle->y0 = 0;
	recycle->width = (uint16_t) width;
	recycle->height = (uint16_t) height;
	recycle->depth = (uint8_t) depth;
	recycle->flags = flags;
	recycle->clipPtr = NULL;
	recycle->basePtr = NULL;
	recycle->tempPtr = NULL;

	if (width != recycle->width || height != recycle->height) {
		// dimension did not fit into 16 bit integer
		gx_destroySurf(recycle);
		return NULL;
	}

	switch (depth) {
		default:
			gx_destroySurf(recycle);
			return NULL;

		case 32:
			recycle->pixeLen = 4;
			break;

		case 24:
			recycle->pixeLen = 3;
			break;

		case 16:
		case 15:
			recycle->pixeLen = 2;
			break;

		case 8:
			recycle->pixeLen = 1;
			break;
	}

	recycle->scanLen = ((unsigned) width * recycle->pixeLen + 3) & ~3;
	if (width > 0 && height > 0 && depth > 0) {
		size_t size = recycle->scanLen * (size_t) height;
		size_t offs = 0;

		switch (flags & SurfType) {
			default:
				break;

			case Surf_2ds:
				break;

			case Surf_pal:
				offs = size;
				size += sizeof(struct gx_Clut);
				break;

			case Surf_fnt:
				offs = size;
				size += sizeof(struct gx_Llut);
				break;

			case Surf_3ds:
				offs = size;
				size += width * (size_t) height * 4;	// z-buffer
				break;
		}

		if ((recycle->basePtr = malloc(size)) == NULL) {
			gx_destroySurf(recycle);
			return NULL;
		}

		recycle->flags |= Surf_freeData;
		if (offs != 0) {
			recycle->tempPtr = recycle->basePtr + offs;
		}
	}

	return recycle;
}
void gx_destroySurf(gx_Surf surf) {
	if (surf->flags & Surf_freeData) {
		surf->flags &= ~Surf_freeData;
		if (surf->basePtr != NULL) {
			free(surf->basePtr);
		}
	}
	surf->basePtr = NULL;
	if (surf->flags & Surf_freeSurf) {
		surf->flags &= ~Surf_freeSurf;
		free(surf);
	}
}

void* gx_cliprect(gx_Surf surf, gx_Rect roi) {
	gx_Clip clp = gx_getclip(surf);

	roi->w += roi->x;
	roi->h += roi->y;

	if (clp->l > roi->x)
		roi->x = clp->l;
	if (clp->t > roi->y)
		roi->y = clp->t;
	if (clp->r < roi->w)
		roi->w = clp->r;
	if (clp->b < roi->h)
		roi->h = clp->b;

	if ((roi->w -= roi->x) <= 0)
		return NULL;

	if ((roi->h -= roi->y) <= 0)
		return NULL;

	return gx_getpaddr(surf, roi->x, roi->y);
}

static int gx_blitSurf(gx_Surf surf, int x, int y, gx_Surf src, gx_Rect roi, cblt_type mode, void *extra) {
	struct gx_Rect clip;
	clip.x = roi ? roi->x : 0;
	clip.y = roi ? roi->y : 0;
	clip.w = roi ? roi->w : src->width;
	clip.h = roi ? roi->h : src->height;

	if (x < 0) {
		clip.x -= x;
		clip.w += x;
	}
	if (y < 0) {
		clip.y -= y;
		clip.h += y;
	}
	char *sptr = gx_cliprect(src, &clip);
	if (sptr == NULL) {
		// there is noting to copy
		return 0;
	}

	clip.x = x;
	clip.y = y;
	if (x < 0) {
		clip.w -= x;
	}
	if (y < 0) {
		clip.h -= y;
	}
	char *dptr = gx_cliprect(surf, &clip);
	if (dptr == NULL) {
		// there are no pixels to set
		return 0;
	}

	cblt_proc blt = gx_getcbltf(mode, src->depth);
	if(blt == NULL) {
		// error: operation is invalid or not implemented
		return -1;
	}

	while (clip.h--) {
		blt(dptr, sptr, extra, (size_t) clip.w);
		dptr += surf->scanLen;
		sptr += src->scanLen;
	}
	return 0;
}

int gx_copySurf(gx_Surf surf, int x, int y, gx_Surf src, gx_Rect roi) {
	return gx_blitSurf(surf, x, y, src, roi, surf->depth, NULL);
}
int gx_lerpSurf(gx_Surf surf, int x, int y, gx_Surf src, gx_Rect roi, int alpha) {
	if (surf->depth != src->depth) {
		// both source and destination must have same depth
		return -2;
	}

	return gx_blitSurf(surf, x, y, src, roi, cblt_cpy_mix, (void *) (ssize_t) alpha);
}
int gx_zoomSurf(gx_Surf surf, gx_Rect rect, gx_Surf src, gx_Rect roi, int interpolate) {
	if (surf->depth != src->depth) {
		return -2;
	}

	struct gx_Rect srec;
	srec.x = srec.y = 0;
	srec.w = src->width;
	srec.h = src->height;
	if (roi != NULL) {
		if (roi->x > 0) {
			srec.w -= roi->x;
			srec.x = roi->x;
		}
		if (roi->y > 0) {
			srec.h -= roi->y;
			srec.y = roi->y;
		}
		if (roi->w < srec.w)
			srec.w = roi->w;
		if (roi->h < srec.h)
			srec.h = roi->h;
	}

	struct gx_Rect drec;
	if (rect != NULL) {
		drec = *rect;
	}
	else {
		drec.x = drec.y = 0;
		drec.w = surf->width;
		drec.h = surf->height;
	}

	if (drec.w <= 0 || drec.h <= 0) {
		return 0;
	}
	if (srec.w <= 0 || srec.h <= 0) {
		return 0;
	}

	int32_t x0 = 0;
	int32_t y0 = 0;
	int32_t dx = (srec.w << 16) / drec.w;
	int32_t dy = (srec.h << 16) / drec.h;

	if (drec.x < 0) {
		x0 = -drec.x;
	}
	if (drec.y < 0) {
		y0 = -drec.y;
	}

	char *dptr = gx_cliprect(surf, &drec);
	if (dptr == NULL) {
		return 0;
	}

	if (!interpolate || dx >= 0x20000 || dy >= 0x20000) {
		for (int y = 0, sy = y0; y < drec.h; ++y, sy += dy) {
			for (int x = 0, sx = x0; x < drec.w; ++x, sx += dx) {
				gx_setpixel(surf, drec.x + x, drec.y + y, gx_getpixel(src, sx >> 16, sy >> 16));
			}
		}
		return 0;
	}

	x0 -= 0x8000;
	y0 -= 0x8000;
	for (int y = 0, sy = y0; y < drec.h; ++y, sy += dy) {
		for (int x = 0, sx = x0; x < drec.w; ++x, sx += dx) {
			gx_setpixel(surf, drec.x + x, drec.y + y, gx_getpix16(src, sx, sy));
		}
	}
	return 0;
}

static inline double gauss(double x, double sigma) {
	double t = x / sigma;
	double SQRT_2_PI_INV = 0.398942280401432677939946059935;
	return SQRT_2_PI_INV * exp(-0.5 * t * t) / sigma;
}
int gx_blurSurf(gx_Surf surf, int radius, double sigma) {
	uint32_t kernelF16[1024];
	double kernel[1024];
	double kernelSum = 0;

	int size = 2 * (radius + 1);
	int mid = radius + 1;

	if (surf->depth != 32) {
		return -1;
	}
	if (size >= 1024) {
		return -1;
	}
	if (size < 1) {
		return 0;
	}

	for (int i = 0; i < size; i += 1) {
		kernel[i] = gauss(mid - i, sigma);
		kernelSum += kernel[i];
	}
	for (int i = 0; i < size; i++) {
		kernelF16[i] = 65536 * (kernel[i] / kernelSum);
	}

	int width = surf->width;
	int height = surf->height;
	gx_Surf tmp = gx_createSurf(NULL, width, height, surf->depth, Surf_2ds);

	if (tmp == NULL) {
		return -1;
	}

	// x direction: inout -> temp
	for (int y = 0; y < height; y += 1) {
		for (int x = 0; x < width; x += 1) {
			uint32_t r = 0;
			uint32_t g = 0;
			uint32_t b = 0;
			for (int i = 0; i < size; i += 1) {
				int _x = x + i - mid;
				uint32_t _k = kernelF16[i];
				if (_x >= 0 && _x < width) {
					uint32_t col = gx_getpixel(surf, _x, y);
					r += _k * rch(col);
					g += _k * gch(col);
					b += _k * bch(col);
				}
			}
			gx_setpixel(tmp, x, y, clamp_srgb(255, r >> 16, g >> 16, b >> 16).val);
		}
	}

	// y direction: temp -> inout
	for (int y = 0; y < height; y += 1) {
		for (int x = 0; x < width; x += 1) {
			uint32_t r = 0;
			uint32_t g = 0;
			uint32_t b = 0;
			for (int i = 0; i < size; i += 1) {
				int _y = y + i - mid;
				uint32_t _k = kernelF16[i];
				if (_y >= 0 || _y < height) {
					uint32_t col = gx_getpixel(tmp, x, _y);
					r += _k * rch(col);
					g += _k * gch(col);
					b += _k * bch(col);
				}
			}
			gx_setpixel(surf, x, y, clamp_srgb(255, r >> 16, g >> 16, b >> 16).val);
		}
	}
	gx_destroySurf(tmp);
	return 0;
}

static inline double f64Max(double a, double b) { return a > b ? a : b; }
static inline double f64Clamp(double t, double a, double b) { return t < a ? a : t > b ? b : t; }

//#{	Gradient fill

struct gradient_data {
	int	sx, sy, x, y;
	double	dx, dy, l, a;
	double (*gf)(struct gradient_data*);
};

static double gradient_lin_factor(struct gradient_data* g) {
	int x = g->x - g->sx;
	int y = g->y - g->sy;
	return g->l * (g->dx * x + g->dy * y);
}
static double gradient_rad_factor(struct gradient_data* g) {
	int x = g->sx - g->x;
	int y = g->sy - g->y;
	return sqrt(x*x*g->dx + y*y*g->dy);
}
static double gradient_sqr_factor(struct gradient_data* g) {
	int x = g->sx - g->x;
	int y = g->sy - g->y;
	return sqrt(f64Max(x * x * g->dx, y * y * g->dy));
}
static double gradient_con_factor(struct gradient_data* g) {
	int x = g->sx - g->x;
	int y = g->sy - g->y;
	double ang = g->a - atan2(x, y);
	if (ang < 0) {
		ang += 2 * M_PI;
	}
	return ang * (1. / (2 * M_PI));
}
static double gradient_spr_factor(struct gradient_data* g) {
	int x = g->sx - g->x;
	int y = g->sy - g->y;
	double ang = atan2(x , y) - g->a;
	double len = g->l * sqrt(x * x + y * y);

	if (ang < 0) {
		ang += 2 * M_PI;
	}
	ang *= 1. / (2 * M_PI);
	return len + ang;
}

int gx_gradSurf(gx_Surf dst, gx_Rect roi, gx_Clut lut, gradient_type gradtype, int repeat) {
	struct gradient_data g;

	if (dst->depth != 32)
		return -1;

	if (roi != NULL) {
		g.sx = roi->x;
		g.sy = roi->y;
		g.x = roi->w ? roi->w : 1;
		g.y = roi->h ? roi->h : 1;
	}
	else {
		g.sx = g.sy = 0;
		g.x = dst->width;
		g.y = dst->height;
	}

	switch (gradtype) {
		default:
			return -3;

		case gradient_lin:
			g.dx = g.x;
			g.dy = g.y;
			g.l = 1. / (g.x * g.x + g.y * g.y);
			g.gf = gradient_lin_factor;
			break;

		case gradient_rad:
			g.dx = 1. / (g.x * g.x);
			g.dy = 1. / (g.y * g.y);
			g.gf = gradient_rad_factor;
			break;

		case gradient_sqr:
			g.dx = 1. / (g.x * g.x);
			g.dy = 1. / (g.y * g.y);
			g.gf = gradient_sqr_factor;
			break;

		case gradient_con:
			g.a = atan2(g.x, g.y) + M_PI;
			g.gf = gradient_con_factor;
			repeat = 1;
			break;

		case gradient_spr:
			g.a = atan2(g.x, g.y) + M_PI;
			g.l = 1. / sqrt(g.x * g.x + g.y * g.y);
			g.gf = gradient_spr_factor;
			repeat = 1;
			break;
	}

	argb *p = (argb *) lut->data;
	gx_Clip clip = gx_getclip(dst);
	char *dptr = (char *) gx_getpaddr(dst, clip->l, clip->t);
	if (dptr == NULL) {
		return -2;
	}

	if (repeat) {
		for (g.y = clip->t; g.y < clip->b; ++g.y) {
			register argb *d = (argb *) dptr;
			for (g.x = clip->l; g.x < clip->r; ++g.x) {
				double len = g.gf(&g);
				len = fmod(len, 1 + 1. / 0x7fffffff);
				*d++ = p[(int) (len * 0xff) & 0xff];
			}
			dptr += dst->scanLen;
		}
	}
	else {
		for (g.y = clip->t; g.y < clip->b; ++g.y) {
			register argb *d = (argb *) dptr;
			for (g.x = clip->l; g.x < clip->r; ++g.x) {
				double len = f64Clamp(g.gf(&g), 0., 1.);
				len = fmod(len, 1 + 1. / 0x7fffffff);
				*d++ = p[(int) (len * 0xff) & 0xff];
			}
			dptr += dst->scanLen;
		}
	}
	return 0;
}

//#}	Gradient fill */
