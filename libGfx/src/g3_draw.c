#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "gx_surf.h"
#include "g3_draw.h"

//~ frustum testing:
static void frustum_get(struct vector planes[6], matrix mat) {
	//~ near and far	: -1 < z < 1
	vecnrm(&planes[0], vecadd(&planes[0], &mat->w, &mat->z));
	vecnrm(&planes[1], vecsub(&planes[1], &mat->w, &mat->z));
	//~ left and right	: -1 < x < 1
	vecnrm(&planes[2], vecadd(&planes[2], &mat->w, &mat->x));
	vecnrm(&planes[3], vecsub(&planes[3], &mat->w, &mat->x));
	//~ top and bottom	: -1 < y < 1
	vecnrm(&planes[4], vecadd(&planes[4], &mat->w, &mat->y));
	vecnrm(&planes[5], vecsub(&planes[5], &mat->w, &mat->y));
}

static vector bsphere(vector s, vector p1, vector p2, vector p3) {
	vecadd(s, vecadd(s, p1, p2), p3);
	vecsca(s, s, (scalar)(1. / 3));									// set s to Baricentrum;

	struct vector tmp[1];
	scalar len = vecdp3(tmp, vecsub(tmp, s, p1));
	scalar ln1 = vecdp3(tmp, vecsub(tmp, s, p2));
	scalar ln2 = vecdp3(tmp, vecsub(tmp, s, p3));
	if (len < ln1) {
		len = ln1;
	}
	if (len < ln2) {
		len = ln2;
	}
	s->w = sqrt(len);
	return s;
}

// returns false if point, sphere or triangle is outside of the frustum
static int ftest_point(struct vector planes[6], vector p) {				// clip point
	const scalar r = 0;
	if (vecdph(p, &planes[0]) <= r) return 0;
	if (vecdph(p, &planes[1]) <= r) return 0;
	if (vecdph(p, &planes[2]) <= r) return 0;
	if (vecdph(p, &planes[3]) <= r) return 0;
	if (vecdph(p, &planes[4]) <= r) return 0;
	if (vecdph(p, &planes[5]) <= r) return 0;
	return 1;	// inside
}
static int ftest_sphere(struct vector planes[6], vector p, scalar r) {			// clip sphere
	if (vecdph(&planes[0], p) <= r) return 0;
	if (vecdph(&planes[1], p) <= r) return 0;
	if (vecdph(&planes[2], p) <= r) return 0;
	if (vecdph(&planes[3], p) <= r) return 0;
	if (vecdph(&planes[4], p) <= r) return 0;
	if (vecdph(&planes[5], p) <= r) return 0;
	return 1;	// inside
}
static int ftest_triangle(struct vector planes[6], vector p1, vector p2, vector p3) {
	struct vector tmp[1];
	bsphere(tmp, p1, p2, p3);
	return ftest_sphere(planes, tmp, -tmp->w);
}

static inline vector mappos(vector dst, matrix mat, vector src) {
	struct vector tmp;
	if (src == dst) {
		src = veccpy(&tmp, src);
	}

	scalar div = vecdph(&mat->w, src);
	if (div != 0) {
		div = 1 / div;
	}
	dst->x = vecdph(&mat->x, src) * div;
	dst->y = vecdph(&mat->y, src) * div;
	dst->z = vecdph(&mat->z, src) * div;
	dst->w = 1;
	return dst;
}

static inline uint32_t sca2fix(double __VAL, int __FIX) {return ((uint32_t)((__VAL) * (1 << (__FIX))));}
static inline uint32_t projx(gx_Surf dst, vector p, int sca) {return sca2fix((dst->width - 1) * (1 + p->x) / 2, sca);}
static inline uint32_t projy(gx_Surf dst, vector p) {return sca2fix((dst->height - 1) * (1 - p->y) / 2, 0);}
static inline uint32_t projz(vector p) {return sca2fix((1 - p->z) / 2, 24);}

static inline int getoffs(gx_Surf dst, int x, int y) {
	return y * (dst->scanLen / 4) + x;
}

void g3_setpixel(gx_Surf dst, int x, int y, unsigned z, uint32_t c) {
	int offs = getoffs(dst, x, y);
	uint32_t *cBuff = (void*)dst->basePtr;
	uint32_t *zBuff = (void*)dst->tempPtr;

	const gx_Clip roi = gx_getclip(dst);
	if (y < roi->ymin || y >= roi->ymax) {
		return;
	}
	if (x < roi->xmin || x >= roi->xmax) {
		return;
	}

	if (zBuff[offs] > z) {
		zBuff[offs] = z;
		cBuff[offs] = c;
	}
}

void g3_putpixel(gx_Surf dst, vector p, int c) {
	int32_t x = projx(dst, p, 0);
	int32_t y = projy(dst, p);
	int32_t z = projz(p);
	g3_setpixel(dst, x, y, z, c);
}


static int calcclip(gx_Clip roi, int x, int y) {
	int c = (y >= roi->ymax) << 0;
	c |= (y < roi->ymin) << 1;
	c |= (x >= roi->xmax) << 2;
	c |= (x < roi->xmin) << 3;
	return c;
}
static int g3_clipline(gx_Clip roi, int *x1, int *y1, int *z1, int *x2, int *y2, int *z2) {
	int c1 = calcclip(roi, *x1, *y1);
	int c2 = calcclip(roi, *x2, *y2);
	int x = 0, dx, y = 0, dy, z = 0, e;

	while (c1 | c2) {

		if (c1 & c2)
			return 0;

		e = c1 ? c1 : c2;
		dx = *x2 - *x1;
		dy = *y2 - *y1;

		switch (LOBIT(e)) {        // get lowest bit set
			case 1:
				y = roi->ymax - 1;
				if (dy != 0) {
					dy = ((y - *y1) << 16) / dy;
				}
				x = *x1 + ((dx * dy) >> 16);
				z = *z1 + ((dx * dy) >> 16);
				break;

			case 2:
				y = roi->ymin;
				if (dy != 0) {
					dy = ((y - *y1) << 16) / dy;
				}
				x = *x1 + ((dx * dy) >> 16);
				z = *z1 + ((dx * dy) >> 16);
				break;

			case 4:
				x = roi->xmax - 1;
				if (dx != 0) {
					dx = ((x - *x1) << 16) / dx;
				}
				y = *y1 + ((dx * dy) >> 16);
				z = *z1 + ((dx * dy) >> 16);
				break;

			case 8:
				x = roi->xmin;
				if (dx != 0) {
					dx = ((x - *x1) << 16) / dx;
				}
				y = *y1 + ((dx * dy) >> 16);
				z = *z1 + ((dx * dy) >> 16);
				break;
		}
		if (c1 != 0) {
			*x1 = x;
			*y1 = y;
			*z1 = z;
			c1 = calcclip(roi, x, y);
		} else {
			*x2 = x;
			*y2 = y;
			*z2 = z;
			c2 = calcclip(roi, x, y);
		}
	}

	return 1;
}

//~ ----------------------------------------------------------------------------

void g3_drawline(gx_Surf dst, vector p1, vector p2, uint32_t c) {		// Bresenham
	int32_t sx = 1, dx;
	int32_t sy = 1, dy;
	int32_t zs = 0, e;

	int x1 = projx(dst, p1, 0);
	int y1 = projy(dst, p1);
	int z1 = projz(p1);

	int x2 = projx(dst, p2, 0);
	int y2 = projy(dst, p2);
	int z2 = projz(p2);

	if ((dy = y2 - y1) < 0) {
		dy = -dy;
		sy = -1;
	}
	if ((dx = x2 - x1) < 0) {
		dx = -dx;
		sx = -1;
	}
	if (!g3_clipline(gx_getclip(dst), &x1, &y1, &z1, &x2, &y2, &z2)) return;

	if (dx > dy) {
		e = dx >> 1;
		zs = (z2 - z1) / dx;
		while (x1 != x2) {
			g3_setpixel(dst, x1, y1, z1, c);
			if ((e += dy) > dx) {
				y1 += sy;
				e -= dx;
			}
			x1 += sx;
			z1 += zs;
		}
	}
	else if (dy) {
		e = dy >> 1;
		zs = (z2 - z1) / dy;
		while (y1 != y2) {
			g3_setpixel(dst, x1, y1, z1, c);
			if ((e += dx) > dy) {
				x1 += sx;
				e -= dy;
			}
			y1 += sy;
			z1 += zs;
		}
	}
}

typedef struct ssds {
	int32_t x;
	int32_t z;
	int32_t s;
	int32_t t;
	argb c;
} *ssds;// */
typedef struct edge {
	int32_t x, dx, z, dz;		//
	int32_t s, ds, t, dt;		// texture
	int32_t r, dr, g, dg, b, db;	// RGB color
	//~ int32_t sz, dsz, tz, dtz, rz, drz;
	//~ float A, dA, S0, S1, T0, T1, Z0, Z1;
	//~ union vector pos, dp;
	//~ union vector nrm, dn;
	//~ union vector col, dc;
	//~ argb rgb;
} *edge;

static inline void edge_next(edge e) {
	e->x += e->dx;
	e->z += e->dz;

	e->s += e->ds, e->t += e->dt;
	e->r += e->dr, e->g += e->dg, e->b += e->db;

	/*
	e->A += e->dA;
	if (draw & text_afine) {
		e->s = scatoi(lrpst(e->S0, e->S1, e->Z0, e->Z1, e->A), 16);
		e->t = scatoi(lrpst(e->T0, e->T1, e->Z0, e->Z1, e->A), 16);

		//~ or better this way ? float O = (1 - e->A) * (1. / e->Z0) + e->A * (1. / e->Z1);
		//~ e->s = scatoi(((1 - e->A) * (e->S0 / e->Z0) + e->A * (e->S1 / e->Z1)) / O, 16);
		//~ e->t = scatoi(((1 - e->A) * (e->T0 / e->Z0) + e->A * (e->T1 / e->Z1)) / O, 16);
	}

	vecadd(&e->pos, &e->pos, &e->dp);
	vecadd(&e->nrm, &e->nrm, &e->dn);
	*/

}
static inline void edge_skip(edge e, int skip) {
	e->x += skip * e->dx;
	e->z += skip * e->dz;

	e->r += skip * e->dr;
	e->g += skip * e->dg;
	e->b += skip * e->db;

	e->s += skip * e->ds;
	e->t += skip * e->dt;

	/*
	e->A += skip * e->dA;

	e->pos.x += skip * e->dp.x;
	e->pos.y += skip * e->dp.y;
	e->pos.z += skip * e->dp.z;
	e->nrm.x += skip * e->dn.x;
	e->nrm.y += skip * e->dn.y;
	e->nrm.z += skip * e->dn.z;
	*/
}
static inline void init_frag(edge e, edge l, edge r, int len, int skip) {
	e->dx = (r->x - (e->x = l->x)) / len;
	e->dz = (r->z - (e->z = l->z)) / len;

	e->ds = (r->s - (e->s = l->s)) / len;
	e->dt = (r->t - (e->t = l->t)) / len;

	e->dr = (r->r - (e->r = l->r)) / len;
	e->dg = (r->g - (e->g = l->g)) / len;
	e->db = (r->b - (e->b = l->b)) / len;

	/*
	e->dA = (1 + (e->A = 0.)) / len;

	e->Z0 = lerpz(l->Z0, l->Z1, l->A);
	e->S0 = lrpst(l->S0, l->S1, l->Z0, l->Z1, l->A);
	e->T0 = lrpst(l->T0, l->T1, l->Z0, l->Z1, l->A);

	e->Z1 = lerpz(r->Z0, r->Z1, r->A);
	e->S1 = lrpst(r->S0, r->S1, r->Z0, r->Z1, r->A);
	e->T1 = lrpst(r->T0, r->T1, r->Z0, r->Z1, r->A);

	e->dp.x = (r->pos.x - (e->pos.x = l->pos.x)) / len;
	e->dp.y = (r->pos.y - (e->pos.y = l->pos.y)) / len;
	e->dp.z = (r->pos.z - (e->pos.z = l->pos.z)) / len;

	e->dn.x = (r->nrm.x - (e->nrm.x = l->nrm.x)) / len;
	e->dn.y = (r->nrm.y - (e->nrm.y = l->nrm.y)) / len;
	e->dn.z = (r->nrm.z - (e->nrm.z = l->nrm.z)) / len;
	*/

	if (skip > 0) {
		edge_skip(e, skip);
	}

}
static inline void init_edge(edge e, ssds s1, ssds s2, int len, int skip) {
	if (len <= 0) {
		e->x = 0;
		return;
	}

	e->dx = (s2->x - (e->x = s1->x)) / len;
	e->dz = (s2->z - (e->z = s1->z)) / len;

	e->dr = ((s2->c.r - s1->c.r) << 16) / len;
	e->dg = ((s2->c.g - s1->c.g) << 16) / len;
	e->db = ((s2->c.b - s1->c.b) << 16) / len;

	e->r = s1->c.r << 16;
	e->g = s1->c.g << 16;
	e->b = s1->c.b << 16;

	e->ds = (s2->s - (e->s = s1->s)) / len;
	e->dt = (s2->t - (e->t = s1->t)) / len;

	/*
	e->dA = (1 + (e->A = 0.)) / len;
	e->Z0 = z1; e->Z1 = z2;
	e->S0 = s1 / 65535.; e->S1 = s2 / 65535.;
	e->T0 = t1 / 65535.; e->T1 = t2 / 65535.;
	*/

	if (skip > 0) {
		edge_skip(e, skip);
	}
}// */

static void draw_tri_part(gx_Surf dst, gx_Clip roi, edge l, edge r, int swap, int y1, int y2, gx_Surf img) {
	if (swap) {
		edge x = l;
		l = r;
		r = x;
	}

	argb *cBuff = (void *) dst->basePtr;
	uint32_t *zBuff = (void *) dst->tempPtr;

	for (int y = y1; y < y2; y++) {
		int lx = l->x >> 16;
		int rx = r->x >> 16;
		int xlr = rx - lx;
		if (xlr > 0) {
			int sx = 0;
			int x = roi->xmin;

			if (rx > roi->xmax) {
				rx = roi->xmax;
			}
			if (lx < x) {
				sx = x - lx;
				lx = x;
			}

			struct edge v;
			init_frag(&v, l, r, xlr, sx);
			int offs = getoffs(dst, lx, y);
			rx = offs + rx - lx;

			while (offs < rx) {
				if (zBuff[offs] > v.z) {
					zBuff[offs] = v.z;
					if (img) {
						argb tex;
						tex.val = gx_getpix16(img, v.s, v.t, 1);
						cBuff[offs].b = clamp_val((v.b >> 15) + tex.b);
						cBuff[offs].g = clamp_val((v.g >> 15) + tex.g);
						cBuff[offs].r = clamp_val((v.r >> 15) + tex.r);
					} else {
						cBuff[offs].b = (uint8_t) (v.b >> 16);
						cBuff[offs].g = (uint8_t) (v.g >> 16);
						cBuff[offs].r = (uint8_t) (v.r >> 16);
					}
				}
				edge_next(&v);
				offs++;
			}
		}
		edge_next(l);
		edge_next(r);
	}
}
static void draw_triangle(gx_Surf dst, vector p, texcol tc, texcol lc, int i1, int i2, int i3, gx_Surf img) {
	// sort by y
	if (p[i1].y < p[i3].y) {
		i1 ^= i3;
		i3 ^= i1;
		i1 ^= i3;
	}
	if (p[i1].y < p[i2].y) {
		i1 ^= i2;
		i2 ^= i1;
		i1 ^= i2;
	}
	if (p[i2].y < p[i3].y) {
		i2 ^= i3;
		i3 ^= i2;
		i2 ^= i3;
	}

	gx_Clip roi = gx_getclip(dst);
	int32_t y1 = projy(dst, p + i1);
	int32_t y2 = projy(dst, p + i2);
	int32_t y3 = projy(dst, p + i3);

	if (y1 < y3) {				// clip (y)
		int y = roi->ymin;
		int dy1 = 0, dy2 = 0;
		int ly1 = 0, ly2 = 0;

		struct edge v1, v2;
		struct ssds s1, s2, s3;

		s1.x = projx(dst, p + i1, 16);
		s2.x = projx(dst, p + i2, 16);
		s3.x = projx(dst, p + i3, 16);

		s1.z = projz(p + i1);
		s2.z = projz(p + i2);
		s3.z = projz(p + i3);

		s1.c.col = s2.c.col = s3.c.col = 0;//x00ffffff;
		s1.s = s2.s = s3.s = 0;
		s1.t = s2.t = s3.t = 0;

		if (img && tc) {
			int w = img->width-1;
			int h = img->height-1;
			s1.s = tc[i1].tex.s * w;
			s1.t = tc[i1].tex.t * h;
			s2.s = tc[i2].tex.s * w;
			s2.t = tc[i2].tex.t * h;
			s3.s = tc[i3].tex.s * w;
			s3.t = tc[i3].tex.t * h;

			//TODO: mip-map selection
			//~ if (y1 == y2) w = X2 - X1;
			//~ else if (y2 == y3) w = X3 - X2;
			//~ else w = X1 < X3 ? X1 - X2 : X3 - X1;
			//~ printf ("tri H:(%+014d), W:(%+014d)\r", y3 - y1, ((w < 0 ? -w : w) >> 16));
			//~ if ((w >>= 16) < 0)w = -w;
			//~ if (tmp > SCRH || ((w < 0 ? -w : w) >> 16) > SCRW) return;
		}
		else if (tc) {
			s1.c = tc[i1].rgb;
			s2.c = tc[i2].rgb;
			s3.c = tc[i3].rgb;
		}

		if (lc) {
			#define litmix(__r, __a, __b) {\
				(__r).r = ((__a).r + (__b).r) / 2;\
				(__r).g = ((__a).g + (__b).g) / 2;\
				(__r).b = ((__a).b + (__b).b) / 2;}
			litmix(s1.c, s1.c, lc[i1]);
			litmix(s2.c, s2.c, lc[i2]);
			litmix(s3.c, s3.c, lc[i3]);
			#undef litmix
		}

		// calc slope y3 - y1 (the longest)
		init_edge(&v1, &s1, &s3, y3 - y1, y - y1);
		ly1 = y2 - y1; dy1 = y - y1;
		ly2 = y3 - y2; dy2 = y - y2;

		// clip y1, y2, y3
		if (y1 < y) {
			if (y2 < y) {
				if (y3 < y) {
					return;
				}
				y2 = y;
			}
			y1 = y;
		}
		if (y3 > (y = roi->ymax)) {
			if (y2 > y) {
				if (y1 > y) {
					return;
				}
				y2 = y;
			}
			y3 = y;
		}

		//~ if (s1.z < 0) return;
		//~ if (s2.z < 0) return;
		//~ if (s3.z < 0) return;
		if (y1 < y2) {				// y1 < y < y2
			init_edge(&v2, &s1, &s2, ly1, dy1);
			draw_tri_part(dst, roi, &v1, &v2, v2.dx < v1.dx, y1, y2, img);
		}
		if (y2 < y3) {				// y2 < y < y3
			init_edge(&v2, &s2, &s3, ly2, dy2);
			draw_tri_part(dst, roi, &v1, &v2, v2.x < v1.x, y2, y3, img);
		}
	}
}

static argb litpos(vector color, vector V, vector N, vector E, gx_Light lit) {
	struct vector tmp[8];
	for (; lit; lit = lit->next) {
		if (!(lit->attr & L_on)) {
			continue;
		}

		vector D = vecsub(tmp+2, V, &lit->pos);
		vector R, L = &lit->dir;
		scalar dotp, attn = 1, spot = 1;

		if (vecdp3(L, L)) {					// directional or spot light
			if (lit->sCos) {			// Spot Light
				spot = vecdp3(L, vecnrm(tmp, D));
				if (spot > cos(toRad(lit->sCos))) {
					spot = pow(spot, lit->sExp);
				}
				else spot = 0;
			}// */
		}
		else {
			L = vecnrm(tmp+3, D);
		}//*/

		attn = spot / vecpev(&lit->attn, veclen(D));

		// Ambient
		vecsca(tmp, &lit->mtl.ambi, attn);
		vecadd(color, color, tmp);
		if ((dotp = -vecdp3(N, L)) > 0) {
			// Diffuse
			vecsca(tmp, &lit->mtl.diff, attn * dotp);
			vecadd(color, color, tmp);

			R = vecnrm(tmp+5, vecrfl(tmp+5, vecsub(tmp+4, V, E), N));
			if ((dotp = -vecdp3(R, L)) > 0) {
				// Specular
				vecsca(tmp, &lit->mtl.spec, attn * pow(dotp, lit->mtl.spow));
				vecadd(color, color, tmp);
			}
		}
	}
	return vecrgb(color);
}

int g3_drawenvc(gx_Surf dst, struct gx_Surf img[6], vector view, matrix proj, double size) {
	#define CLPNRM 1
	const scalar e = 0;
	struct vector v[8], nrm[1];
	struct texcol t[8];

	v[4].x = v[7].x = v[3].x = v[0].x = -size;
	v[1].x = v[2].x = v[5].x = v[6].x = +size;
	v[1].y = v[5].y = v[4].y = v[0].y = -size;
	v[3].y = v[2].y = v[7].y = v[6].y = +size;
	v[1].z = v[2].z = v[3].z = v[0].z = -size;
	v[4].z = v[5].z = v[7].z = v[6].z = +size;

	mappos(v + 0, proj, v + 0);
	mappos(v + 1, proj, v + 1);
	mappos(v + 2, proj, v + 2);
	mappos(v + 3, proj, v + 3);
	mappos(v + 4, proj, v + 4);
	mappos(v + 5, proj, v + 5);
	mappos(v + 6, proj, v + 6);
	mappos(v + 7, proj, v + 7);

	if (CLPNRM || vecdp3(view, vecldf(nrm, -1, 0, 0, 0)) < e) {
		t[1].s = 0x0000; t[1].t = 0xffff;
		t[2].s = 0x0000; t[2].t = 0x0000;
		t[6].s = 0xffff; t[6].t = 0x0000;
		t[5].s = 0xffff; t[5].t = 0xffff;
		draw_triangle(dst, v, t, 0, 1, 2, 6, img + 2);
		draw_triangle(dst, v, t, 0, 1, 6, 5, img + 2);
	}
	if (CLPNRM || vecdp3(view, vecldf(nrm, +1, 0, 0, 0)) < e) {
		t[0].s = 0xffff; t[0].t = 0xffff;
		t[3].s = 0xffff; t[3].t = 0x0000;
		t[7].s = 0x0000; t[7].t = 0x0000;
		t[4].s = 0x0000; t[4].t = 0xffff;
		draw_triangle(dst, v, t, 0, 0,3,7, img + 3);
		draw_triangle(dst, v, t, 0, 0,7,4, img + 3);
	}
	if (CLPNRM || vecdp3(view, vecldf(nrm, 0, +1, 0, 0)) < e) {
		t[0].s = 0x0000; t[0].t = 0x0000;
		t[1].s = 0xffff; t[1].t = 0x0000;
		t[5].s = 0xffff; t[5].t = 0xffff;
		t[4].s = 0x0000; t[4].t = 0xffff;
		draw_triangle(dst, v, t, 0, 0,1,5, img + 5);
		draw_triangle(dst, v, t, 0, 0,5,4, img + 5);
	}
	if (CLPNRM || vecdp3(view, vecldf(nrm, 0, -1, 0, 0)) < e) {
		t[2].s = 0xffff; t[2].t = 0xffff;
		t[3].s = 0x0000; t[3].t = 0xffff;
		t[7].s = 0x0000; t[7].t = 0x0000;
		t[6].s = 0xffff; t[6].t = 0x0000;
		draw_triangle(dst, v, t, 0, 2,3,7, img + 4);
		draw_triangle(dst, v, t, 0, 2,7,6, img + 4);
	}
	if (CLPNRM || vecdp3(view, vecldf(nrm, 0, 0, +1, 0)) < e) {
		t[0].s = 0x0000; t[0].t = 0xffff;
		t[1].s = 0xffff; t[1].t = 0xffff;
		t[2].s = 0xffff; t[2].t = 0x0000;
		t[3].s = 0x0000; t[3].t = 0x0000;
		draw_triangle(dst, v, t, 0, 0,1,2, img + 0);
		draw_triangle(dst, v, t, 0, 0,2,3, img + 0);
	}
	if (CLPNRM || vecdp3(view, vecldf(nrm, 0, 0, -1, 0)) < e) {
		t[4].s = 0xffff; t[4].t = 0xffff;
		t[5].s = 0x0000; t[5].t = 0xffff;
		t[6].s = 0x0000; t[6].t = 0x0000;
		t[7].s = 0xffff; t[7].t = 0x0000;
		draw_triangle(dst, v, t, 0, 4,5,6, img + 1);
		draw_triangle(dst, v, t, 0, 4,6,7, img + 1);
	}

	return 0;
}// */

int g3_drawMesh(gx_Surf dst, gx_Mesh msh, matrix objm, camera cam, int draw) {
	gx_Surf img = NULL;
	const int32_t line_col = 0xffffff;

	long i, tricnt = 0;
	struct vector v[8];
	struct matrix tmp[3];
	matrix proj, view;
	gx_Light l;

	#define MAXVTX (65536*16)
	static struct vector pos[MAXVTX];
	static struct texcol tmpcolarr[MAXVTX];
	texcol lit = 0, col = tmpcolarr;

	//~ objm = NULL;
	if (msh->vtxcnt > MAXVTX) {
		return -1;
	}

	view = cammat(tmp, cam);

	if (objm != NULL) {
		view = matmul(tmp + 1, tmp, objm);
	}
	proj = matmul(tmp + 2, &cam->proj, view);

	if (draw & zero_cbuf) {
		int *cBuff = (void*)dst->basePtr;
		for (int e = 0; e < dst->width * dst->height; e += 1)
			cBuff[e] = 0x00000000;
	}
	if (draw & zero_zbuf) {
		int *zBuff = (void*)dst->tempPtr;
		for (int e = 0; e < dst->width * dst->height; e += 1)
			zBuff[e] = 0x3fffffff;
	}

	if (draw & draw_tex) {
		img = msh->map;
		col = msh->tex;
	}
	if (draw & draw_lit) {
		lit = tmpcolarr;
	}

	for (l = msh->lit; l; l = l->next) {
		// pre multiply lights (fore back)
		vecmul(&l->mtl.ambi, &msh->mtl.ambi, &l->ambi);
		vecmul(&l->mtl.diff, &msh->mtl.diff, &l->diff);
		vecmul(&l->mtl.spec, &msh->mtl.spec, &l->spec);
		l->mtl.spow = msh->mtl.spow;
	}

	for (i = 0; i < msh->vtxcnt; i += 1) {		// calc

		mappos(pos + i, proj, msh->pos + i);

		if (lit) {
			struct vector Col = msh->mtl.emis;
			vector V = msh->pos + i;
			vector N = msh->nrm + i;	// normalVec
			if (objm != NULL) {
				V = matvph(v+1, objm, V);	// vertexPos
				N = matvph(v+0, objm, N);	// vertexPos
			}
			//~ const vector V = matvph(v+1, objm, msh->pos + i);	// vertexPos
			//~ const vector N = matvp3(v+0, objm, msh->nrm + i);	// normalVec
			tmpcolarr[i].rgb = litpos(&Col, V, N, &cam->pos, msh->lit);
		}

		else if ((draw & draw_tex) != 0) {
			if (msh->hasTex && img != NULL) {
				int s = col[i].s * (img->width - 1);
				int t = col[i].t * (img->height - 1);
				tmpcolarr[i].val = gx_getpix16(img, s, t, 0);
			}
			else {
				tmpcolarr[i] = msh->tex[i];
			}
		}
		else {
			tmpcolarr[i].rgb = nrmrgb(msh->nrm + i);
		}
	}

	if (dst == NULL) {
		return 0;
	}

	frustum_get(v, proj);

	for (i = 0; i < msh->tricnt; i += 1) {		// draw
		int32_t i1 = msh->triptr[i].i1;
		int32_t i2 = msh->triptr[i].i2;
		int32_t i3 = msh->triptr[i].i3;

		if (!ftest_triangle(v, msh->pos + i1, msh->pos + i2, msh->pos + i3)) {
			continue;
		}

		// drop degenerated triangles
		if (pos[i1].w <= 0 || pos[i2].w <= 0 || pos[i3].w <= 0) {
			continue;
		}

		switch (draw & cull_mode) {				// culling faces(FIXME)
			default: return 0;
			case 0: break;
			case cull_front:
			case cull_back: {
				vector f = pos;
				float fA = (f[i2].x - f[i1].x) * (f[i3].y - f[i1].y);
				float fB = (f[i2].y - f[i1].y) * (f[i3].x - f[i1].x);
				switch (draw & cull_mode) {
					case cull_back:
						if (fA >= fB) {
							continue;
						}
						break;
					case cull_front:
						if (fA <= fB) {
							continue;
						}
						break;
				}
			}
		}
		switch (draw & draw_mode) {
			default:
				return 0;
			case draw_plot: {
				g3_putpixel(dst, pos + i1, col[i1].val);
				g3_putpixel(dst, pos + i2, col[i2].val);
				g3_putpixel(dst, pos + i3, col[i3].val);
			} break;
			case draw_wire: {
				g3_drawline(dst, pos + i1, pos + i2, col[i1].val);
				g3_drawline(dst, pos + i2, pos + i3, col[i2].val);
				g3_drawline(dst, pos + i1, pos + i3, col[i3].val);
			} break;
			case draw_fill: {
				draw_triangle(dst, pos, col, lit, i1, i2, i3, img);
			} break;
		}
		tricnt += 1;
	}
	for (i = 0; i < msh->segcnt; i += 1) {		// draw segs
		int32_t i1 = msh->segptr[i].p1;
		int32_t i2 = msh->segptr[i].p2;
		g3_drawline(dst, pos + i1, pos + i2, line_col);
	}

	return tricnt;
}

int g3_drawBbox(gx_Surf dst, gx_Mesh msh, matrix objm, camera cam) {
	const int32_t bbox_col = 0xff00ff;

	//~ unsigned i, tricnt = 0;
	struct vector v[8];
	struct matrix tmp[3];
	matrix proj, view;

	//~ World*Wiew*Proj
	view = cammat(tmp, cam);

	if (objm)
		view = matmul(tmp + 1, tmp, objm);

	proj = matmul(tmp + 2, &cam->proj, view);

	bboxMesh(msh, v + 0, v + 6);
	v[4].x = v[7].x = v[3].x = v[0].x;
	v[1].x = v[2].x = v[5].x = v[6].x;
	v[1].y = v[5].y = v[4].y = v[0].y;
	v[3].y = v[2].y = v[7].y = v[6].y;
	v[1].z = v[2].z = v[3].z = v[0].z;
	v[4].z = v[5].z = v[7].z = v[6].z;

	mappos(v + 0, proj, v + 0);
	mappos(v + 1, proj, v + 1);
	mappos(v + 2, proj, v + 2);
	mappos(v + 3, proj, v + 3);
	mappos(v + 4, proj, v + 4);
	mappos(v + 5, proj, v + 5);
	mappos(v + 6, proj, v + 6);
	mappos(v + 7, proj, v + 7);

	// front
	g3_drawline(dst, v + 0, v + 1, bbox_col);
	g3_drawline(dst, v + 1, v + 2, bbox_col);
	g3_drawline(dst, v + 2, v + 3, bbox_col);
	g3_drawline(dst, v + 3, v + 0, bbox_col);

	// back
	g3_drawline(dst, v + 4, v + 5, bbox_col);
	g3_drawline(dst, v + 5, v + 6, bbox_col);
	g3_drawline(dst, v + 6, v + 7, bbox_col);
	g3_drawline(dst, v + 7, v + 4, bbox_col);

	// join
	g3_drawline(dst, v + 0, v + 4, bbox_col);
	g3_drawline(dst, v + 1, v + 5, bbox_col);
	g3_drawline(dst, v + 2, v + 6, bbox_col);
	g3_drawline(dst, v + 3, v + 7, bbox_col);
	return 0;
}
