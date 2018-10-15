#include "gx_surf.h"
#include "g3_math.h"

enum {
	//~ draw_none = 0x00000000,		//
	draw_plot = 0x00000001,		//
	draw_wire = 0x00000002,		//
	draw_fill = 0x00000003,		//
	draw_mode = 0x00000003,		// mask

	zero_cbuf = 0x00000004,		// clear color buffer
	zero_zbuf = 0x00000008,		// clear z buffer

	//~ cull_none = 0x00000000,		// no cull
	cull_back = 0x00000010,		// front face
	cull_front= 0x00000020,		// back face
	cull_all  = 0x00000030,		// back & front face
	cull_mode = 0x00000030,		// enable culling

	draw_tex  = 0x00000040,		// use texture
	draw_lit  = 0x00000080,		// use lights
};

typedef struct texcol {
	union {
		struct {
			uint16_t s;
			uint16_t t;
		};
		struct {
			uint16_t s;
			uint16_t t;
		} tex;
		struct {
			uint8_t b;
			uint8_t g;
			uint8_t r;
			uint8_t a;
		};
		uint32_t val;
		argb rgb;
	};
} *texcol;

typedef struct gx_Material {
	struct vector emis;		// Emissive
	struct vector ambi;		// Ambient
	struct vector diff;		// Diffuse
	struct vector spec;		// Specular
	scalar spow;			// Shininess
} *gx_Material;

typedef struct gx_Light {
	struct gx_Light *next;
	struct gx_Material mtl;

	struct vector ambi;		// Ambient
	struct vector diff;		// Diffuse
	struct vector spec;		// Specular
	struct vector attn;		// Attenuation
	struct vector pos;		// position
	struct vector dir;		// direction

	scalar sCos, sExp;			// spot light related
	enum {						// Type
		L_off  = 0x0000,		// light is off
		L_on   = 0x0001,		// light is on
		//~ L_attn = 0x0002,		// attenuate
		//~ L_dir  = 0x0004,		// is directional
		//~ L_spot = 0x0008,		// is directional
	} attr;
} *gx_Light;

typedef struct gx_Mesh {
	gx_Light lit;				// mesh lights?
	gx_Surf map;			// texture map
	struct gx_Material mtl;	// back, fore?

	long maxvtx, vtxcnt;	// vertices
	long maxtri, tricnt;	// triangles
	long maxseg, segcnt;	// segments
	//size_t maxgrp, grpcnt;	// grouping
	struct vector *pos;		// (x, y, z, 1) position
	struct vector *nrm;		// (x, y, z, 0) normal
	struct texcol *tex;		// (s, t, 0, 0) texture|color

	struct tri {			// triangle list
		// signed id;	// groupId
		long i1;
		long i2;
		long i3;
	} *triptr;
	struct seg {			// line list
		long p1;
		long p2;
	} *segptr;
	/* groups
	struct grp {			// group list
		signed t1;			// first triangle in triptr
		signed t2;			// last triangle
		signed parent;		// parent group of group
		matrix transform;	// transformation matrix
		gx_Surf map;		// texture map
		gx_Material mtl;	// back, fore?
	} *grpptr; */

	uint32_t freeMesh:1;
	uint32_t freeMap:1;
	uint32_t hasTex:1;// := map != null
	uint32_t hasNrm:1;// := nrm != null
	uint32_t padded:29;// padding
} *gx_Mesh;

gx_Mesh g3_createMesh(gx_Mesh recycle, size_t n);
void g3_destroyMesh(gx_Mesh msh);

int g3_readObj(gx_Mesh msh, const char *file);
int g3_read3ds(gx_Mesh msh, const char *file);
int g3_saveObj(gx_Mesh msh, const char *file);

void bboxMesh(gx_Mesh msh, vector min, vector max);
//void centMesh(gx_Mesh msh, scalar size);
void normMesh(gx_Mesh msh, scalar tol, vector center, vector resize);

long setvtx(gx_Mesh msh, long idx, vector pos, vector nrm, scalar tex[2]);
static inline long addvtx(gx_Mesh msh, vector pos, vector nrm, scalar tex[2]) {
	return setvtx(msh, msh->vtxcnt, pos, nrm, tex);
}

long addseg(gx_Mesh msh, long p1, long p2);
long addtri(gx_Mesh msh, long p1, long p2, long p3);
long addquad(gx_Mesh msh, long p1, long p2, long p3, long p4);


void g3_drawline(gx_Surf dst, vector p1, vector p2, uint32_t c);

int g3_drawMesh(gx_Surf dst, gx_Mesh msh, matrix objm, camera cam, int mode);

int g3_drawBbox(gx_Surf dst, gx_Mesh msh, matrix objm, camera cam);

int g3_drawenvc(gx_Surf dst, struct gx_Surf img[6], vector view, matrix proj, double size);
