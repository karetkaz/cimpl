#include "gx_surf.h"
#include "g3_math.h"

struct gradient_data {
	int	sx, sy, x, y;
	double	dx, dy, l, a;
	double (*gf)(struct gradient_data*);
};

static inline double f64Max(double a, double b) { return a > b ? a : b; }
static inline double f64Clamp(double t) { return t < 0 ? 0 : t > 1 ? 1 : t; }
static inline double f64Modulus(double t) { t = fmod(t, 1); return t < 0 ? 1 + t : t; }

static double gradientLinear(struct gradient_data* g) {
	int x = g->x - g->sx;
	int y = g->y - g->sy;
	return f64Clamp(g->l * (g->dx * x + g->dy * y));
}
static double gradientLinearRepeat(struct gradient_data* g) {
	int x = g->x - g->sx;
	int y = g->y - g->sy;
	return f64Modulus(g->l * (g->dx * x + g->dy * y));
}
static double gradientRadial(struct gradient_data* g) {
	int x = g->x - g->sx;
	int y = g->y - g->sy;
	return f64Clamp(sqrt(x * x * g->dx + y * y * g->dy));
}
static double gradientRadialRepeat(struct gradient_data* g) {
	int x = g->x - g->sx;
	int y = g->y - g->sy;
	return f64Modulus(sqrt(x * x * g->dx + y * y * g->dy));
}
static double gradientSquare(struct gradient_data* g) {
	int x = g->x - g->sx;
	int y = g->y - g->sy;
	return f64Clamp(sqrt(f64Max(x * x * g->dx, y * y * g->dy)));
}
static double gradientSquareRepeat(struct gradient_data* g) {
	int x = g->x - g->sx;
	int y = g->y - g->sy;
	return f64Modulus(sqrt(f64Max(x * x * g->dx, y * y * g->dy)));
}
static double gradientConic(struct gradient_data* g) {
	int x = g->sx - g->x;
	int y = g->sy - g->y;
	double ang = g->a - atan2(x, y);
	if (ang < 0) {
		ang += 2 * M_PI;
	}
	return f64Modulus(ang * (1. / (2 * M_PI)));
}
static double gradientSpiral(struct gradient_data* g) {
	int x = g->x - g->sx;
	int y = g->y - g->sy;
	double ang = atan2(x, y) - g->a;
	double len = g->l * sqrt(x * x + y * y);

	if (ang < 0) {
		ang += 2 * M_PI;
	}
	ang *= 1. / (2 * M_PI);
	return f64Modulus(len + ang);
}

int drawGradient(GxImage dst, GxRect roi, GradientFlags type, int length, uint32_t *colors) {
	struct gradient_data g;

	if (dst->depth != 32)
		return -1;

	if (roi != NULL) {
		g.sx = roi->x;
		g.sy = roi->y;
		g.x = roi->w;
		g.y = roi->h;
	}
	else {
		g.sx = g.sy = 0;
		g.x = dst->width;
		g.y = dst->height;
	}

	switch (type & mask_type) {
		default:
			return -3;

		case gradient_lin:
			g.dx = g.x;
			g.dy = g.y;
			g.l = 1. / (g.x * g.x + g.y * g.y);
			if ((type & flag_repeat) != 0) {
				g.gf = gradientLinearRepeat;
			} else {
				g.gf = gradientLinear;
			}
			break;

		case gradient_rad:
			g.x /= 2;
			g.y /= 2;
			g.sx += g.x;
			g.sy += g.y;
			g.dx = 1. / (g.x * g.x);
			g.dy = 1. / (g.y * g.y);
			if ((type & flag_repeat) != 0) {
				g.gf = gradientRadialRepeat;
			} else {
				g.gf = gradientRadial;
			}
			break;

		case gradient_sqr:
			g.x /= 2;
			g.y /= 2;
			g.sx += g.x;
			g.sy += g.y;
			g.dx = 1. / (g.x * g.x);
			g.dy = 1. / (g.y * g.y);
			if ((type & flag_repeat) != 0) {
				g.gf = gradientSquareRepeat;
			} else {
				g.gf = gradientSquare;
			}
			break;

		case gradient_con:
			g.x /= 2;
			g.y /= 2;
			g.sx += g.x;
			g.sy += g.y;
			g.a = atan2(g.x, g.y) + M_PI;
			g.gf = gradientConic;
			break;

		case gradient_spr:
			g.x /= 2;
			g.y /= 2;
			g.sx += g.x;
			g.sy += g.y;
			g.a = atan2(g.x, g.y) + M_PI;
			g.l = 1. / sqrt(g.x * g.x + g.y * g.y);
			g.gf = gradientSpiral;
			break;
	}

	GxClip clip = getClip(dst);
	char *dptr = (char *) getPAddr(dst, clip->l, clip->t);
	if (dptr == NULL) {
		return -2;
	}

	if (type & flag_alpha) {
		for (g.y = clip->t; g.y < clip->b; ++g.y) {
			argb *d = (argb *) dptr;
			for (g.x = clip->l; g.x < clip->r; ++g.x) {
				int idx = length * g.gf(&g);
				if (idx >= length) {
					idx = length - 1;
				}
				else if (idx < 0) {
					idx = 0;
				}
				d->a = colors[idx];
				d += 1;
			}
			dptr += dst->scanLen;
		}
		return 0;
	}

	for (g.y = clip->t; g.y < clip->b; ++g.y) {
		argb *d = (argb *) dptr;
		for (g.x = clip->l; g.x < clip->r; ++g.x) {
			int idx = length * g.gf(&g);
			if (idx >= length) {
				idx = length - 1;
			}
			else if (idx < 0) {
				idx = 0;
			}
			d->val = colors[idx];
			d += 1;
		}
		dptr += dst->scanLen;
	}
	return 0;
}
