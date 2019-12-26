#include "gx_surf.h"
#include "g3_math.h"

enum {
	draw_plot = 0x00000001,		//
	draw_wire = 0x00000002,		//
	draw_fill = 0x00000003,		//
	draw_mode = 0x00000003,		// mask

	zero_cbuf = 0x00000004,		// clear color buffer
	zero_zbuf = 0x00000008,		// clear z buffer

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
	gx_Surf map;			// texture map
	struct gx_Material mtl;	// back, fore?

	size_t maxvtx, vtxcnt;	// vertices
	size_t maxtri, tricnt;	// triangles
	size_t maxseg, segcnt;	// polygons
	//size_t maxgrp, grpcnt;	// groups
	struct vector *pos;		// (x, y, z, 1) position
	struct vector *nrm;		// (x, y, z, 0) normal
	struct texcol *tex;		// (s, t, 0, 0) texture|color

	struct tri {			// triangle list
		// signed id;	// groupId
		size_t i1;
		size_t i2;
		size_t i3;
	} *triptr;
	struct seg {			// line list
		size_t p1;
		size_t p2;
	} *segptr;
	/* groups
	struct grp {			// group list
		signed t1;			// first triangle
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
} *gx_Mesh;

gx_Mesh g3_createMesh(gx_Mesh recycle, size_t n);
void g3_destroyMesh(gx_Mesh msh);

int g3_readObj(gx_Mesh msh, const char *file);
int g3_read3ds(gx_Mesh msh, const char *file);
int g3_saveObj(gx_Mesh msh, const char *file);

/**
 * get the axis aligned bounding box of the mesh
 * @param msh mesh
 * @param min output vector containing the min values (x, y, z)
 * @param max output vector containing the max values (x, y, z)
 */
void bboxMesh(gx_Mesh msh, vector min, vector max);

/**
 * recalculate mesh normals
 * @param msh mesh
 * @param tol tolerance
 * @param center (optional) translate the mesh
 * @param resize (optional) resize the mesh
 */
void normMesh(gx_Mesh msh, scalar tol, vector center, vector resize);

/**
 * update one vertex of the mesh at the given index
 * @param msh mesh to be updated
 * @param idx index of vertex
 * @param pos update position
 * @param nrm update normal
 * @param tex update texture position
 * @return 0 if failed
 */
int setvtx(gx_Mesh msh, size_t idx, scalar pos[3], scalar nrm[3], scalar tex[2]);

/**
 * Add a new vertex to the mesh
 * @param msh mesh to be updated
 * @param pos update position
 * @param nrm update normal
 * @param tex update texture position
 * @return 0 if failed
 */
static inline int addvtx(gx_Mesh msh, scalar pos[3], scalar nrm[3], scalar tex[2]) {
	return setvtx(msh, msh->vtxcnt, pos, nrm, tex);
}

/**
 * connect 2 points with a segment (line) in the mesh
 * @param msh mesh to be updated
 * @param p1 index of the first vertex
 * @param p2 index of the second vertex
 * @return 0 if not possible
 */
int addseg(gx_Mesh msh, size_t p1, size_t p2);

/**
 * connect 3 points (triangle) in the mesh
 * @param msh mesh to be updated
 * @param p1 index of the first vertex
 * @param p2 index of the second vertex
 * @param p3 index of the third vertex
 * @return 0 if not possible
 */
int addtri(gx_Mesh msh, size_t p1, size_t p2, size_t p3);

/**
 * connect 4 points (add 2 triangles) in the mesh
 * @param msh mesh to be updated
 * @param p1 index of the first vertex
 * @param p2 index of the second vertex
 * @param p3 index of the third vertex
 * @param p4 index of the fourth vertex
 * @return 0 if not possible
 */
static inline int addquad(gx_Mesh msh, size_t p1, size_t p2, size_t p3, size_t p4) {
	if (!addtri(msh, p1, p2, p3)) {
		return 0;
	}
	return addtri(msh, p1, p3, p4);
}

void g3_drawline(gx_Surf dst, vector p1, vector p2, uint32_t c);
int g3_drawMesh(gx_Surf dst, gx_Mesh msh, matrix objm, camera cam, gx_Light lights, int mode);
int g3_drawBbox(gx_Surf dst, gx_Mesh msh, matrix objm, camera cam);
int g3_drawenvc(gx_Surf dst, struct gx_Surf img[6], vector view, matrix proj, double size);

// extract the planes (near, far, left, right, top, bottom) from the projection matrix
void frustum_get(struct vector planes[6], matrix proj);

// test if a point, sphere or triangle is inside or outside of the frustum
int ftest_point(struct vector planes[6], vector p);
int ftest_sphere(struct vector planes[6], vector p, scalar r);
int ftest_triangle(struct vector planes[6], vector p1, vector p2, vector p3);
