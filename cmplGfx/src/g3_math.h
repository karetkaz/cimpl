/* tiny vector and matrix library */

#ifndef __3D_MATH
#define __3D_MATH

#include "gx_color.h"

#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288419716939937510582097494459
#endif

typedef float scalar;

typedef struct vector {
	union {
	scalar v[4];
	struct {
		scalar x;
		scalar y;
		scalar z;
		scalar w;
	};
	struct {
		scalar r;
		scalar g;
		scalar b;
		scalar a;
	};
	};
} *vector;

typedef struct matrix {
	union {
	scalar d[16];
	scalar m[4][4];
	struct {
		struct vector x;
		struct vector y;
		struct vector z;
		struct vector w;
	};
	struct {
		scalar m11, m12, m13, m14;
		scalar m21, m22, m23, m24;
		scalar m31, m32, m33, m34;
		scalar m41, m42, m43, m44;
	};
	};
} *matrix;

typedef struct camera {
	struct matrix proj;			// projection matrix

	struct vector dirR;			// camera right direction
	struct vector dirU;			// camera up direction
	struct vector dirF;			// camera forward direction
	struct vector pos;			// camera Location
} *camera;

typedef enum {				// swizzle
	xxxx = 0x00, yxxx = 0x01, zxxx = 0x02, wxxx = 0x03, xyxx = 0x04, yyxx = 0x05, zyxx = 0x06, wyxx = 0x07,
	xzxx = 0x08, yzxx = 0x09, zzxx = 0x0a, wzxx = 0x0b, xwxx = 0x0c, ywxx = 0x0d, zwxx = 0x0e, wwxx = 0x0f,
	xxyx = 0x10, yxyx = 0x11, zxyx = 0x12, wxyx = 0x13, xyyx = 0x14, yyyx = 0x15, zyyx = 0x16, wyyx = 0x17,
	xzyx = 0x18, yzyx = 0x19, zzyx = 0x1a, wzyx = 0x1b, xwyx = 0x1c, ywyx = 0x1d, zwyx = 0x1e, wwyx = 0x1f,
	xxzx = 0x20, yxzx = 0x21, zxzx = 0x22, wxzx = 0x23, xyzx = 0x24, yyzx = 0x25, zyzx = 0x26, wyzx = 0x27,
	xzzx = 0x28, yzzx = 0x29, zzzx = 0x2a, wzzx = 0x2b, xwzx = 0x2c, ywzx = 0x2d, zwzx = 0x2e, wwzx = 0x2f,
	xxwx = 0x30, yxwx = 0x31, zxwx = 0x32, wxwx = 0x33, xywx = 0x34, yywx = 0x35, zywx = 0x36, wywx = 0x37,
	xzwx = 0x38, yzwx = 0x39, zzwx = 0x3a, wzwx = 0x3b, xwwx = 0x3c, ywwx = 0x3d, zwwx = 0x3e, wwwx = 0x3f,
	xxxy = 0x40, yxxy = 0x41, zxxy = 0x42, wxxy = 0x43, xyxy = 0x44, yyxy = 0x45, zyxy = 0x46, wyxy = 0x47,
	xzxy = 0x48, yzxy = 0x49, zzxy = 0x4a, wzxy = 0x4b, xwxy = 0x4c, ywxy = 0x4d, zwxy = 0x4e, wwxy = 0x4f,
	xxyy = 0x50, yxyy = 0x51, zxyy = 0x52, wxyy = 0x53, xyyy = 0x54, yyyy = 0x55, zyyy = 0x56, wyyy = 0x57,
	xzyy = 0x58, yzyy = 0x59, zzyy = 0x5a, wzyy = 0x5b, xwyy = 0x5c, ywyy = 0x5d, zwyy = 0x5e, wwyy = 0x5f,
	xxzy = 0x60, yxzy = 0x61, zxzy = 0x62, wxzy = 0x63, xyzy = 0x64, yyzy = 0x65, zyzy = 0x66, wyzy = 0x67,
	xzzy = 0x68, yzzy = 0x69, zzzy = 0x6a, wzzy = 0x6b, xwzy = 0x6c, ywzy = 0x6d, zwzy = 0x6e, wwzy = 0x6f,
	xxwy = 0x70, yxwy = 0x71, zxwy = 0x72, wxwy = 0x73, xywy = 0x74, yywy = 0x75, zywy = 0x76, wywy = 0x77,
	xzwy = 0x78, yzwy = 0x79, zzwy = 0x7a, wzwy = 0x7b, xwwy = 0x7c, ywwy = 0x7d, zwwy = 0x7e, wwwy = 0x7f,
	xxxz = 0x80, yxxz = 0x81, zxxz = 0x82, wxxz = 0x83, xyxz = 0x84, yyxz = 0x85, zyxz = 0x86, wyxz = 0x87,
	xzxz = 0x88, yzxz = 0x89, zzxz = 0x8a, wzxz = 0x8b, xwxz = 0x8c, ywxz = 0x8d, zwxz = 0x8e, wwxz = 0x8f,
	xxyz = 0x90, yxyz = 0x91, zxyz = 0x92, wxyz = 0x93, xyyz = 0x94, yyyz = 0x95, zyyz = 0x96, wyyz = 0x97,
	xzyz = 0x98, yzyz = 0x99, zzyz = 0x9a, wzyz = 0x9b, xwyz = 0x9c, ywyz = 0x9d, zwyz = 0x9e, wwyz = 0x9f,
	xxzz = 0xa0, yxzz = 0xa1, zxzz = 0xa2, wxzz = 0xa3, xyzz = 0xa4, yyzz = 0xa5, zyzz = 0xa6, wyzz = 0xa7,
	xzzz = 0xa8, yzzz = 0xa9, zzzz = 0xaa, wzzz = 0xab, xwzz = 0xac, ywzz = 0xad, zwzz = 0xae, wwzz = 0xaf,
	xxwz = 0xb0, yxwz = 0xb1, zxwz = 0xb2, wxwz = 0xb3, xywz = 0xb4, yywz = 0xb5, zywz = 0xb6, wywz = 0xb7,
	xzwz = 0xb8, yzwz = 0xb9, zzwz = 0xba, wzwz = 0xbb, xwwz = 0xbc, ywwz = 0xbd, zwwz = 0xbe, wwwz = 0xbf,
	xxxw = 0xc0, yxxw = 0xc1, zxxw = 0xc2, wxxw = 0xc3, xyxw = 0xc4, yyxw = 0xc5, zyxw = 0xc6, wyxw = 0xc7,
	xzxw = 0xc8, yzxw = 0xc9, zzxw = 0xca, wzxw = 0xcb, xwxw = 0xcc, ywxw = 0xcd, zwxw = 0xce, wwxw = 0xcf,
	xxyw = 0xd0, yxyw = 0xd1, zxyw = 0xd2, wxyw = 0xd3, xyyw = 0xd4, yyyw = 0xd5, zyyw = 0xd6, wyyw = 0xd7,
	xzyw = 0xd8, yzyw = 0xd9, zzyw = 0xda, wzyw = 0xdb, xwyw = 0xdc, ywyw = 0xdd, zwyw = 0xde, wwyw = 0xdf,
	xxzw = 0xe0, yxzw = 0xe1, zxzw = 0xe2, wxzw = 0xe3, xyzw = 0xe4, yyzw = 0xe5, zyzw = 0xe6, wyzw = 0xe7,
	xzzw = 0xe8, yzzw = 0xe9, zzzw = 0xea, wzzw = 0xeb, xwzw = 0xec, ywzw = 0xed, zwzw = 0xee, wwzw = 0xef,
	xxww = 0xf0, yxww = 0xf1, zxww = 0xf2, wxww = 0xf3, xyww = 0xf4, yyww = 0xf5, zyww = 0xf6, wyww = 0xf7,
	xzww = 0xf8, yzww = 0xf9, zzww = 0xfa, wzww = 0xfb, xwww = 0xfc, ywww = 0xfd, zwww = 0xfe, wwww = 0xff
} vswzop;

static inline scalar toRad(scalar deg) { return deg * (M_PI / 180); }

//#################################  COLOR8  ###################################
/*/#{
typedef union color8_t {				// bool vect || byte vect || argb
	unsigned int32_t val;
	unsigned int8_t v[4];
	struct {
		unsigned char	b;
		unsigned char	g;
		unsigned char	r;
		unsigned char	a;
	};
	struct {
		signed char	x;
		signed char	y;
		signed char	z;
		signed char	w;
	};
} bvec, argb;

argb rgbneg(argb rhs) {
	argb res;
	res.b = -rhs.b;
	res.g = -rhs.g;
	res.r = -rhs.r;
	res.a = -rhs.a;
	return res;
}

argb argbnot(argb rhs) {
	argb res;
	res.val = ~rhs.val;
	return res;
}

argb argband(argb lhs, argb rhs) {
	argb res;
	res.val = lhs.val & rhs.val;
	return res;
}

argb argbior(argb lhs, argb rhs) {
	argb res;
	res.val = lhs.val | rhs.val;
	return res;
}

argb argbxor(argb lhs, argb rhs) {
	argb res;
	res.val = lhs.val ^ rhs.val;
	return res;
}

argb argbadd(argb lhs, argb rhs) {
	argb res;
	register int tmp;
	res.b = (tmp = lhs.b + rhs.b) > 255 ? 255 : tmp;
	res.g = (tmp = lhs.g + rhs.g) > 255 ? 255 : tmp;
	res.r = (tmp = lhs.r + rhs.r) > 255 ? 255 : tmp;
	res.a = (tmp = lhs.a + rhs.a) > 255 ? 255 : tmp;
	return res;
}

argb argbsub(argb lhs, argb rhs) {
	argb res;
	register int tmp;
	res.b = (tmp = lhs.b - rhs.b) < 0 ? 0 : tmp;
	res.g = (tmp = lhs.g - rhs.g) < 0 ? 0 : tmp;
	res.r = (tmp = lhs.r - rhs.r) < 0 ? 0 : tmp;
	res.a = (tmp = lhs.a - rhs.a) < 0 ? 0 : tmp;
	return res;
}

argb argbmul(argb lhs, argb rhs) {
	argb res;
	register int tmp;
	res.b = ((tmp = lhs.b * rhs.b) + (tmp >> 8) + 1) >> 8;
	res.g = ((tmp = lhs.g * rhs.g) + (tmp >> 8) + 1) >> 8;
	res.r = ((tmp = lhs.r * rhs.r) + (tmp >> 8) + 1) >> 8;
	res.a = ((tmp = lhs.a * rhs.a) + (tmp >> 8) + 1) >> 8;
	return res;
}

argb argbdiv(argb lhs, argb rhs) {
	argb res;
	res.b = (lhs.b << 8) / (rhs.b + 1);
	res.g = (lhs.g << 8) / (rhs.g + 1);
	res.r = (lhs.r << 8) / (rhs.r + 1);
	res.a = (lhs.a << 8) / (rhs.a + 1);
	return res;
}

argb argbdif(argb lhs, argb rhs) {
	argb res;
	res.b = lhs.b - rhs.b;
	res.g = lhs.g - rhs.g;
	res.r = lhs.r - rhs.r;
	res.a = lhs.a - rhs.a;
	return res;
}

argb argbmin(argb lhs, argb rhs) {
	argb res;
	res.b = lhs.b < rhs.b ? lhs.b : rhs.b;
	res.g = lhs.g < rhs.g ? lhs.g : rhs.g;
	res.r = lhs.r < rhs.r ? lhs.r : rhs.r;
	res.a = lhs.a < rhs.a ? lhs.a : rhs.a;
	return res;
}

argb argbmax(argb lhs, argb rhs) {
	argb res;
	res.b = lhs.b > rhs.b ? lhs.b : rhs.b;
	res.g = lhs.g > rhs.g ? lhs.g : rhs.g;
	res.r = lhs.r > rhs.r ? lhs.r : rhs.r;
	res.a = lhs.a > rhs.a ? lhs.a : rhs.a;
	return res;
}

argb argbmix(argb lhs, argb rhs, unsigned char cnt) {
	argb res;
	register int tmp;
	res.b = rhs.b + (((tmp = (lhs.b - rhs.b) * cnt) + (tmp >> 8) + 1) >> 8);
	res.g = rhs.g + (((tmp = (lhs.g - rhs.g) * cnt) + (tmp >> 8) + 1) >> 8);
	res.r = rhs.r + (((tmp = (lhs.r - rhs.r) * cnt) + (tmp >> 8) + 1) >> 8);
	res.a = rhs.a + (((tmp = (lhs.a - rhs.a) * cnt) + (tmp >> 8) + 1) >> 8);
	return res;
}

argb argbscr(argb lhs, argb rhs) {
	argb res;
	res.b = 255 - ((255 - lhs.b) * (255 - rhs.b + 1) >> 8);
	res.g = 255 - ((255 - lhs.g) * (255 - rhs.g + 1) >> 8);
	res.r = 255 - ((255 - lhs.r) * (255 - rhs.r + 1) >> 8);
	res.a = 255 - ((255 - lhs.a) * (255 - rhs.a + 1) >> 8);
	return res;
}

argb argbovr(argb lhs, argb rhs) {
	argb res;
	res.b = lhs.b <= 127 ? (lhs.b * (1 + rhs.b)) >> 7 : 255 - (((255 - lhs.b) * (256 - rhs.b)) >> 7);
	res.g = lhs.g <= 127 ? (lhs.g * (1 + rhs.g)) >> 7 : 255 - (((255 - lhs.g) * (256 - rhs.g)) >> 7);
	res.r = lhs.r <= 127 ? (lhs.r * (1 + rhs.r)) >> 7 : 255 - (((255 - lhs.r) * (256 - rhs.r)) >> 7);
	res.a = lhs.a <= 127 ? (lhs.a * (1 + rhs.a)) >> 7 : 255 - (((255 - lhs.a) * (256 - rhs.a)) >> 7);
	return res;
}
//#} */
//#################################  VECTOR  ###################################

static inline vector veclds(vector dst, scalar s) {
	dst->x = s;
	dst->y = s;
	dst->z = s;
	dst->w = s;
	return dst;
}

static inline vector vecldf(vector dst, scalar x, scalar y, scalar z, scalar w) {
	dst->x = x;
	dst->y = y;
	dst->z = z;
	dst->w = w;
	return dst;
}

static inline vector vecld4(vector dst, vector xyz, scalar w) {
	dst->x = xyz->x;
	dst->y = xyz->y;
	dst->z = xyz->z;
	dst->w = w;
	return dst;
}

static inline vector veccpy(vector dst, vector src) {
	dst->x = src->x;
	dst->y = src->y;
	dst->z = src->z;
	dst->w = src->w;
	return dst;
}// */

static inline vector vecneg(vector dst, vector src) {
	dst->x = -src->x;
	dst->y = -src->y;
	dst->z = -src->z;
	dst->w = -src->w;
	return dst;
}

static inline vector vecabs(vector dst, vector src) {
	dst->x = src->x >= 0 ? src->x : -src->x;
	dst->y = src->y >= 0 ? src->y : -src->y;
	dst->z = src->z >= 0 ? src->z : -src->z;
	dst->w = src->w >= 0 ? src->w : -src->w;
	return dst;
}

static inline vector vecrcp(vector dst, vector src) {
	dst->x = src->x ? 1. / src->x : 0;
	dst->y = src->y ? 1. / src->y : 0;
	dst->z = src->z ? 1. / src->z : 0;
	dst->w = src->w ? 1. / src->w : 0;
	return dst;
}

static inline vector vecsat(vector dst, vector src) {
	if (src->x > 1) dst->x = 1; else if (src->x < 0) dst->x = 0; else dst->x = src->x;
	if (src->y > 1) dst->y = 1; else if (src->y < 0) dst->y = 0; else dst->y = src->y;
	if (src->z > 1) dst->z = 1; else if (src->z < 0) dst->z = 0; else dst->z = src->z;
	if (src->w > 1) dst->w = 1; else if (src->w < 0) dst->w = 0; else dst->w = src->w;
	return dst;
}

static inline scalar vecdp3(vector lhs, vector rhs) {
	return lhs->x * rhs->x + lhs->y * rhs->y + lhs->z * rhs->z;
}

static inline scalar vecdph(vector lhs, vector rhs) {
	return lhs->x * rhs->x + lhs->y * rhs->y + lhs->z * rhs->z + lhs->w;
}

static inline scalar vecdp4(vector lhs, vector rhs) {
	return lhs->x * rhs->x + lhs->y * rhs->y + lhs->z * rhs->z + lhs->w * rhs->w;
}

static inline vector vecmin(vector dst, vector lhs, vector rhs) {
	dst->x = lhs->x < rhs->x ? lhs->x : rhs->x;
	dst->y = lhs->y < rhs->y ? lhs->y : rhs->y;
	dst->z = lhs->z < rhs->z ? lhs->z : rhs->z;
	dst->w = lhs->w < rhs->w ? lhs->w : rhs->w;
	return dst;
}

static inline vector vecmax(vector dst, vector lhs, vector rhs) {
	dst->x = lhs->x > rhs->x ? lhs->x : rhs->x;
	dst->y = lhs->y > rhs->y ? lhs->y : rhs->y;
	dst->z = lhs->z > rhs->z ? lhs->z : rhs->z;
	dst->w = lhs->w > rhs->w ? lhs->w : rhs->w;
	return dst;
}

static inline vector vecadd(vector dst, vector lhs, vector rhs) {
	dst->x = lhs->x + rhs->x;
	dst->y = lhs->y + rhs->y;
	dst->z = lhs->z + rhs->z;
	dst->w = lhs->w + rhs->w;
	return dst;
}

static inline vector vecsub(vector dst, vector lhs, vector rhs) {
	dst->x = lhs->x - rhs->x;
	dst->y = lhs->y - rhs->y;
	dst->z = lhs->z - rhs->z;
	dst->w = lhs->w - rhs->w;
	return dst;
}

static inline vector vecmul(vector dst, vector lhs, vector rhs) {
	dst->x = lhs->x * rhs->x;
	dst->y = lhs->y * rhs->y;
	dst->z = lhs->z * rhs->z;
	dst->w = lhs->w * rhs->w;
	return dst;
}

static inline vector veccrs(vector dst, vector lhs, vector rhs) {
	struct vector tmp;
	tmp.x = lhs->y * rhs->z - lhs->z * rhs->y;
	tmp.y = lhs->z * rhs->x - lhs->x * rhs->z;
	tmp.z = lhs->x * rhs->y - lhs->y * rhs->x;
	return *dst = tmp, dst;
}

static inline vector vecsca(vector dst, vector lhs, scalar rhs) {
	dst->x = lhs->x * rhs;
	dst->y = lhs->y * rhs;
	dst->z = lhs->z * rhs;
	dst->w = lhs->w * rhs;
	return dst;
}

static inline vector vecmov(vector dst, vector src, vswzop swz) {
	struct vector tmp;
	if (src == dst)
		tmp = *src, src = &tmp;
	dst->x = src->v[(swz >> 0) & 3];
	dst->y = src->v[(swz >> 2) & 3];
	dst->z = src->v[(swz >> 4) & 3];
	dst->w = src->v[(swz >> 6) & 3];
	return dst;
}

static inline vector vecnrm(vector dst, vector src) {
	scalar len = vecdp3(src, src);
	if (len) len = 1. / sqrt(len);
	dst->x = src->x * len;
	dst->y = src->y * len;
	dst->z = src->z * len;
	dst->w = src->w * len;
	return dst;
}

static inline struct vector vecldc(argb col) {
	struct vector result;
	result.r = col.r / 255.;
	result.g = col.g / 255.;
	result.b = col.b / 255.;
	result.a = col.a / 255.;
	return result;
}

static inline struct vector vec(scalar x, scalar y, scalar z, scalar w) {
	struct vector result;
	result.x = x;
	result.y = y;
	result.z = z;
	result.w = w;
	return result;
}

static inline argb vecrgb(vector src) {
	return clamp_srgb(src->a * 255, src->r * 255, src->g * 255, src->b * 255);
}

static inline argb nrmrgb(vector src) {
	return clamp_srgb((src->a + 1.) * 127, (src->r + 1.) * 127, (src->g + 1.) * 127, (src->b + 1.) * 127);
}

static inline vector vecrfl(vector dst, vector dir, vector nrm) {	// reflect
	return vecsub(dst, dir, vecsca(dst, nrm, 2 * vecdp3(nrm, dir)));
}// */
//~ static inline vector vecrfr(vector dst, vector dir, vector nrm);	// refract

static inline scalar vecpev(vector pol, scalar val) {				// polinomial evaluate
	return ((((pol->w) * val + pol->z) * val + pol->y) * val) + pol->x;
}

static inline scalar veclen(vector src) {
	return sqrt(vecdp3(src, src));
}

static inline scalar vecdst(vector lhs, vector rhs) {
	struct vector tmp[1];
	return veclen(vecsub(tmp, lhs, rhs));
}

/* Temp
inline vector vecHCS(vector dst, vector src) {
	scalar len = src->w;
	if (len) len = 1. / len;
	dst->x *= len;
	dst->y *= len;
	dst->z *= len;
	dst->w *= len;
	return dst;
}

//? inline vector vecABS(vector dst, vector src);	| a |		// max(-src, src)
//+ inline vector vecRCP(vector dst, vector src);	1 / a
//~ inline vector vecRSQ(vector dst, vector src);			// <=> vecPOW(dst, src, <-1/2, -1/2, -1/2, -1/2>)

//? inline vector vecFLR(vector dst, vector src);		//
//? inline vector vecFRC(vector dst, vector src);

//+ inline vector vecSIN(vector dst, vector src);
//+ inline vector vecCOS(vector dst, vector src);
//+ inline vector vecLG2(vector dst, vector src);
//+ inline vector vecEX2(vector dst, vector src);
//? inline vector vecPOW(vector dst, vector lhs, vector rhs);				// Exp2(rhs * Log2(lhs))
//+ inline vector vecLRP(vector dst, vector lhs, vector rhs, vector cnt);		// lhs*(cnt) + rhs*(1-cnt)

//~ inline vector vecLIT(vector dst, ???);

//~ inline vector vecSGE(vector dst, vector lhs, vector rhs);	// dst.* = lhs.* >= rhs.* ? 0 : 1
//~ inline vector vecSLT(vector dst, vector lhs, vector rhs);	// dst.* = lhs.* < rhs.* ? 0 : 1
//~ inline vector vecMAD(vector dst, vector src, vector mul, vector add);

asm(
	"movaps (%%eax), %%xmm0\n"
	"addps (%%ecx), %%xmm0\n"
	"movaps %%xmm0, (%%edx)\n"
	: // output
	: "a"(lhs), "c"(rhs), "d"(dst)// input
	: "%xmm0"// no clobber-list
);

#pragma aux (parm [edi] [eax] [edx] value [edi] modify exact [xmm0]) vecadd =\
	".686"\
	"movaps	xmm0, [eax]"\
	"addps	xmm0, [edx]"\
	"movaps	[edi], xmm0"//

*/

static inline scalar backface(vector N, vector p1, vector p2, vector p3) {
	struct vector e1, e2;
	if (N == NULL) {
		N = &e1;
	}
	vecsub(&e1, p2, p1);
	vecsub(&e2, p3, p1);
	vecnrm(N, veccrs(N, &e1, &e2));
	N->w = 0;
	return vecdp3(N, p1);
}


//#################################  MATRIX  ###################################
static inline matrix matidn(matrix dst, scalar v) {
	dst->m11 = v; dst->m12 = 0; dst->m13 = 0; dst->m14 = 0;
	dst->m21 = 0; dst->m22 = v; dst->m23 = 0; dst->m24 = 0;
	dst->m31 = 0; dst->m32 = 0; dst->m33 = v; dst->m34 = 0;
	dst->m41 = 0; dst->m42 = 0; dst->m43 = 0; dst->m44 = v;
	return dst;
}

static inline matrix matadd(matrix dst, matrix lhs, matrix rhs) {
	vecadd(&dst->x, &lhs->x, &rhs->x);
	vecadd(&dst->y, &lhs->y, &rhs->y);
	vecadd(&dst->z, &lhs->z, &rhs->z);
	vecadd(&dst->w, &lhs->w, &rhs->w);
	return dst;
}

static inline matrix matsub(matrix dst, matrix lhs, matrix rhs) {
	vecsub(&dst->x, &lhs->x, &rhs->x);
	vecsub(&dst->y, &lhs->y, &rhs->y);
	vecsub(&dst->z, &lhs->z, &rhs->z);
	vecsub(&dst->w, &lhs->w, &rhs->w);
	return dst;
}

static inline matrix matsca(matrix dst, matrix lhs, scalar rhs) {
	vecsca(&dst->x, &lhs->x, rhs);
	vecsca(&dst->y, &lhs->y, rhs);
	vecsca(&dst->z, &lhs->z, rhs);
	vecsca(&dst->w, &lhs->w, rhs);
	return dst;
}

static inline matrix matmul(matrix dst, matrix lhs, matrix rhs) {
	struct matrix tmp;
	int row, col;
	for(row = 0; row < 4; ++row) {
		for(col = 0; col < 4; ++col) {
			tmp.m[row][col] = \
				lhs->m[row][0] * rhs->m[0][col]+
				lhs->m[row][1] * rhs->m[1][col]+
				lhs->m[row][2] * rhs->m[2][col]+
				lhs->m[row][3] * rhs->m[3][col];
		}
	}
	*dst = tmp;
	return dst;//matcpy(dst, &tmp);
}

static inline vector matvp3(vector dst, matrix mat, vector src) {
	struct vector tmp;
	if (src == dst)
		tmp = *src, src = &tmp;
	dst->x = vecdp3(&mat->x, src);
	dst->y = vecdp3(&mat->y, src);
	dst->z = vecdp3(&mat->z, src);
	dst->w = 1;//vecdp3(&mat->w, src);
	return dst;
}

static inline vector matvp4(vector dst, matrix mat, vector src) {
	struct vector tmp;
	if (src == dst)
		tmp = *src, src = &tmp;
	dst->x = vecdp4(&mat->x, src);
	dst->y = vecdp4(&mat->y, src);
	dst->z = vecdp4(&mat->z, src);
	dst->w = vecdp4(&mat->w, src);
	return dst;
}

static inline vector matvph(vector dst, matrix mat, vector src) {
	struct vector tmp;
	if (src == dst)
		tmp = *src, src = &tmp;
	dst->x = vecdph(&mat->x, src);
	dst->y = vecdph(&mat->y, src);
	dst->z = vecdph(&mat->z, src);
	dst->w = vecdph(&mat->w, src);
	return dst;
}

static inline matrix matldf(matrix dst
			, scalar _11, scalar _12, scalar _13, scalar _14\
			, scalar _21, scalar _22, scalar _23, scalar _24\
			, scalar _31, scalar _32, scalar _33, scalar _34\
			, scalar _41, scalar _42, scalar _43, scalar _44) {
	dst->m11 = _11; dst->m12 = _12; dst->m13 = _13; dst->m14 = _14;
	dst->m21 = _21; dst->m22 = _22; dst->m23 = _23; dst->m24 = _24;
	dst->m31 = _31; dst->m32 = _32; dst->m33 = _33; dst->m34 = _34;
	dst->m41 = _41; dst->m42 = _42; dst->m43 = _43; dst->m44 = _44;
	return dst;
}

static inline matrix matldR(matrix dst, vector dir, scalar angle) {
	float len = veclen(dir);
	float x = dir->x / len;
	float y = dir->y / len;
	float z = dir->z / len;
	float cx = 0;
	float cy = 0;
	float cz = 0;
	float xx = x * x;
	float xy = x * y;
	float xz = x * z;
	float yy = y * y;
	float yz = y * z;
	float zz = z * z;

	float s = sin(angle);
	float c = cos(angle);
	float k = 1 - c;

	dst->m11 = xx + (yy + zz) * c;
	dst->m12 = xy * k - z * s;
	dst->m13 = xz * k + y * s;
	dst->m14 = (cx * (yy + zz) - x * (cy * y + cz * z)) * k + (cy * z - cz * y) * s;

	dst->m21 = xy * k + z * s;
	dst->m22 = yy + (xx + zz) * c;
	dst->m23 = yz * k - x * s;
	dst->m24 = (cy * (xx + zz) - y * (cx * x + cz * z)) * k + (cz * x - cx * z) * s;

	dst->m31 = xz * k - y * s;
	dst->m32 = yz * k + x * s;
	dst->m33 = zz + (xx + yy) * c;
	dst->m34 = (cz * (xx + yy) - z * (cx * x + cy * y)) * k + (cx * y - cy * x) * s;

	dst->m41 = 0;
	dst->m42 = 0;
	dst->m43 = 0;
	dst->m44 = 1;

	return dst;
}

static inline matrix matldS(matrix dst, vector dir, scalar cnt) {
	struct vector tmp;
	matidn(dst, 1);
	vecsca(&tmp, dir, cnt);
	dst->x.x = tmp.x;
	dst->y.y = tmp.y;
	dst->z.z = tmp.z;
	//~ dst->w.w = tmp.w;
	return dst;
}

static inline matrix matldT(matrix dst, vector dir, scalar cnt) {
	struct vector tmp;
	matidn(dst, 1);
	vecsca(&tmp, dir, cnt);
	dst->x.w = tmp.x;
	dst->y.w = tmp.y;
	dst->z.w = tmp.z;
	//~ dst->w.w = tmp.w;
	return dst;
}

static inline void ortho_mat(matrix dst, scalar l, scalar r, scalar b, scalar t, scalar n, scalar f) {
	scalar rl = r - l;
	scalar tb = t - b;
	scalar nf = n - f;

	if (rl == 0. || tb  == 0. || nf  == 0.)
		return;

	matldf(dst,			// Projection matrix - orthographic
		2 / rl,			0.,				0.,				-(r+l) / rl,
		0.,				2 / tb,			0.,				-(t+b) / tb,
		0.,				0.,				2 / nf,			-(f+n) / nf,
		0.,				0.,				0.,				1.);

	/* step by step
	//~ union matrix tmp;
	matldf(dst,							// scale
		2/(r-l),		0.,				0.,				0.,
		0.,				2/(t-b),		0.,				0.,
		0.,				0.,				2/(n-f),		0.,
		0.,				0.,				0.,				1.);
	matmul(dst, dst, matldf(&tmp,		// translate
		1.,				0.,				0.,				-(l+r)/2,
		0.,				1.,				0.,				-(t+b)/2,
		0.,				0.,				1.,				-(n+f)/2,
		0.,				0.,				0.,				1.));
	// */
}

static inline void persp_mat(matrix dst, scalar l, scalar r, scalar b, scalar t, scalar n, scalar f) {
	scalar rl = r - l;
	scalar tb = t - b;
	scalar nf = n - f;

	if (rl == 0. || tb  == 0. || nf  == 0.)
		return;

	matldf(dst,			// Projection matrix - perspective
		2*n / rl,	0.,			-(r+l) / rl,	0.,
		0.,			2*n / tb,	-(t+b) / tb,	0.,
		0.,			0.,			+(n+f) / nf,	-2*n*f / nf,
		0.,			0.,			1.,				0.);

	/* step by step
	//~ union matrix tmp;
	matldf(dst,							// scale
		2/(r-l),		0.,				0.,				0.,
		0.,				2/(t-b),		0.,				0.,
		0.,				0.,				2/(n-f),		0.,
		0.,				0.,				0.,				1.);
	matmul(dst, dst, matldf(&tmp,		// translate
		1.,				0.,				0.,				-(l+r)/2,
		0.,				1.,				0.,				-(t+b)/2,
		0.,				0.,				1.,				-(n+f)/2,
		0.,				0.,				0.,				1.));
	//~ --- Ortographic ---
	matmul(dst, dst, matldf(&tmp,		// perspective
		n,				0.,				0.,				0,
		0.,				n,				0.,				0,
		0.,				0.,				n+f,			-n*f,
		0.,				0.,				1.,				0.));
	// */
}

static inline void projv_mat(matrix dst, scalar fovy, scalar asp, scalar n, scalar f) {
	scalar bot = 1;
	scalar nf = n - f;
	if (fovy) {		// perspective
		bot = tan(fovy * ((3.14159265358979323846 / 180)));
		asp *= bot;

		matldf(dst,
			n / asp,	0.,		0.,		0,
			0.,		n / bot,	0.,		0,
			0.,		0.,		(n+f) / nf,	-2*n*f / nf,
			0.,		0.,		1.,		0);
	}
	else {			// orthographic
		matldf(dst,
			1 / asp,	0.,		0.,		0,
			0.,		1 / bot,	0.,		0,
			0.,		0.,		2 / nf,		-(f+n) / nf,
			0.,		0.,		0.,		1.);
	}
}


static inline matrix matcpy(matrix dst, matrix src) {
	*dst = *src;
	return dst;
}

static inline matrix matran(matrix dst, matrix src) {
	struct matrix tmp;
	int row, col;
	if (src == dst)
		src = matcpy(&tmp, src);//~ {*tmp = src; rsc = &tmp;}
	for(row = 0; row < 4; ++row)
		for(col = 0; col < 4; ++col)
			tmp.m[col][row] = src->m[row][col];
	return matcpy(dst, &tmp);
}

static scalar det3x3(scalar x1, scalar x2, scalar x3, scalar y1, scalar y2, scalar y3, scalar z1, scalar z2, scalar z3) {
	return	x1 * (y2 * z3 - z2 * y3)-
			x2 * (y1 * z3 - z1 * y3)+
			x3 * (y1 * z2 - z1 * y2);
}

static inline scalar matdet(matrix src) {
	scalar* x = (scalar*)&src->x.v;
	scalar* y = (scalar*)&src->y.v;
	scalar* z = (scalar*)&src->z.v;
	scalar* w = (scalar*)&src->w.v;
	return	x[0] * det3x3(y[1], y[2], y[3], z[1], z[2], z[3], w[1], w[2], w[3])-
		x[1] * det3x3(y[0], y[2], y[3], z[0], z[2], z[3], w[0], w[2], w[3])+
		x[2] * det3x3(y[0], y[1], y[3], z[0], z[1], z[3], w[0], w[1], w[3])-
		x[3] * det3x3(y[0], y[1], y[2], z[0], z[1], z[2], w[0], w[1], w[2]);
}

static inline matrix matadj(matrix dst, matrix src) {
	struct matrix tmp;
	scalar* x = (scalar*)&tmp.x.v;
	scalar* y = (scalar*)&tmp.y.v;
	scalar* z = (scalar*)&tmp.z.v;
	scalar* w = (scalar*)&tmp.w.v;
	matran(&tmp, src);
	dst->m11 = +det3x3(y[1], y[2], y[3], z[1], z[2], z[3], w[1], w[2], w[3]);
	dst->m12 = -det3x3(y[0], y[2], y[3], z[0], z[2], z[3], w[0], w[2], w[3]);
	dst->m13 = +det3x3(y[0], y[1], y[3], z[0], z[1], z[3], w[0], w[1], w[3]);
	dst->m14 = -det3x3(y[0], y[1], y[2], z[0], z[1], z[2], w[0], w[1], w[2]);
	dst->m21 = -det3x3(x[1], x[2], x[3], z[1], z[2], z[3], w[1], w[2], w[3]);
	dst->m22 = +det3x3(x[0], x[2], x[3], z[0], z[2], z[3], w[0], w[2], w[3]);
	dst->m23 = -det3x3(x[0], x[1], x[3], z[0], z[1], z[3], w[0], w[1], w[3]);
	dst->m24 = +det3x3(x[0], x[1], x[2], z[0], z[1], z[2], w[0], w[1], w[2]);
	dst->m31 = +det3x3(x[1], x[2], x[3], y[1], y[2], y[3], w[1], w[2], w[3]);
	dst->m32 = -det3x3(x[0], x[2], x[3], y[0], y[2], y[3], w[0], w[2], w[3]);
	dst->m33 = +det3x3(x[0], x[1], x[3], y[0], y[1], y[3], w[0], w[1], w[3]);
	dst->m34 = -det3x3(x[0], x[1], x[2], y[0], y[1], y[2], w[0], w[1], w[2]);
	dst->m41 = -det3x3(x[1], x[2], x[3], y[1], y[2], y[3], z[1], z[2], z[3]);
	dst->m42 = +det3x3(x[0], x[2], x[3], y[0], y[2], y[3], z[0], z[2], z[3]);
	dst->m43 = -det3x3(x[0], x[1], x[3], y[0], y[1], y[3], z[0], z[1], z[3]);
	dst->m44 = +det3x3(x[0], x[1], x[2], y[0], y[1], y[2], z[0], z[1], z[2]);
	return dst;
}

static inline scalar matinv(matrix dst, matrix src) {
	scalar det = matdet(src);
	if (det) {
		matadj(dst, src);
		matsca(dst, dst, 1. / det);
	}
	return det;
}

//#################################  camera  ###################################

// look at ...
static inline void camset(camera cam, vector eye, vector tgt, vector up) {
	struct vector tmp;
	vecnrm(&cam->dirF, vecsub(&tmp, tgt, eye));
	vecnrm(&cam->dirR, veccrs(&tmp, up, &tmp));
	veccrs(&cam->dirU, &cam->dirF, &cam->dirR);
	veccpy(&cam->pos, eye);
}

// move the camera ...
static inline void cammov(camera cam, vector dir, scalar cnt) {
	struct vector tmp;
	vecsca(&tmp, dir, cnt);
	vecadd(&cam->pos, &cam->pos, &tmp);
}

// rotate the camera ...
static inline void camrot(camera cam, vector dir, vector orbit, scalar ang) {
	struct matrix tmp;
	if (ang == 0)
			return;
	matldR(&tmp, dir, ang);
	vecnrm(&cam->dirF, matvph(&cam->dirF, &tmp, &cam->dirF));
	vecnrm(&cam->dirR, matvph(&cam->dirR, &tmp, &cam->dirR));
	veccrs(&cam->dirU, &cam->dirF, &cam->dirR);
	if (orbit != NULL) {
		struct vector dir[1];
		scalar dist = veclen(vecsub(dir, orbit, &cam->pos));
		#if 1	// camera will just rotate around orbit
			vecnrm(dir, matvph(dir, &tmp, dir));
			matldT(&tmp, dir, -dist);
		#else	// camera will rotate and look at orbit
			matldT(&tmp, &cam->dirF, -dist);
		#endif
		matvph(&cam->pos, &tmp, orbit);
	}
}

static inline matrix cammat(matrix mat, camera cam) {
	vecld4(&mat->x, &cam->dirR, -vecdp3(&cam->dirR, &cam->pos));
	vecld4(&mat->y, &cam->dirU, -vecdp3(&cam->dirU, &cam->pos));
	vecld4(&mat->z, &cam->dirF, -vecdp3(&cam->dirF, &cam->pos));
	vecldf(&mat->w, 0., 0., 0., 1.);
	return mat;
}

#endif
