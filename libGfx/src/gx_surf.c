#include <math.h>
#include <stdlib.h>
#include "gx_color.h"
#include "gx_surf.h"

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
		size_t size = recycle->scanLen * height;
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
				size += width * height * 4;
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
