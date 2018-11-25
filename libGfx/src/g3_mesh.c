#include "g3_draw.h"

#include <stdlib.h>
#include <string.h>

static inline int HIBIT(unsigned int x) {
	int res = 0;
	if (x != 0) {
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
		res = x - (x >> 1);
	}
	return res;
}

static char* readKVP(char *ptr, char *key, char *sep, char *wsp) {	// key = value pair
	if (wsp == NULL) wsp = " \t";

	// match key and skip white spaces
	if (*key) {
		while (*key && *key == *ptr) ++key, ++ptr;
		if (!sep && !strchr(wsp, *ptr)) return 0;
		while (*ptr && strchr(wsp, *ptr)) ++ptr;
	}

	// match sep and skip white spaces
	if (sep && *sep) {
		while (*sep && *sep == *ptr) ++sep, ++ptr;
		while (*ptr && strchr(wsp, *ptr)) ++ptr;
	}

	// if key and separator were mached
	return (*key || (sep && *sep)) ? NULL : ptr;
}
static char* readI32(char *ptr, int *outVal) {
	int sgn = 1, val = 0;
	if (!strchr("+-0123456789", *ptr))
		return ptr;
	if (*ptr == '-' || *ptr == '+') {
		sgn = *ptr == '-' ? -1 : +1;
		ptr += 1;
	}
	while (*ptr >= '0' && *ptr <= '9') {
		val = val * 10 + (*ptr - '0');
		ptr += 1;
	}
	if (outVal) *outVal = sgn * val;
	return ptr;
}

gx_Mesh g3_createMesh(gx_Mesh recycle, size_t n) {
	if (recycle == NULL) {
		recycle = malloc(sizeof(struct gx_Mesh));
		if (recycle == NULL) {
			gx_debug("allocation failed");
			return NULL;
		}
		recycle->freeMesh = 1;
	}
	else {
		free(recycle->triptr);
		free(recycle->segptr);
		free(recycle->pos);
		free(recycle->nrm);
		free(recycle->tex);
		if (recycle->freeMap) {
			free(recycle->map);
		}
		recycle->triptr = NULL;
		recycle->segptr = NULL;
		recycle->pos = NULL;
		recycle->nrm = NULL;
		recycle->tex = NULL;
		recycle->map = NULL;
	}

	recycle->freeMap = 0;
	recycle->map = NULL;  // TODO: gx_createSurf(512, 512, 32, 2ds);

	recycle->tricnt = 0;
	recycle->maxtri = n <= 0 ? 16 : n;
	recycle->triptr = (struct tri*)malloc(sizeof(struct tri) * recycle->maxtri);

	recycle->segcnt = 0;
	recycle->maxseg = n <= 0 ? 16 : n;
	recycle->segptr = (struct seg*)malloc(sizeof(struct seg) * recycle->maxseg);

	recycle->vtxcnt = 0;
	recycle->maxvtx = n <= 0 ? 16 : n;
	recycle->pos = malloc(sizeof(struct vector) * recycle->maxvtx);
	recycle->nrm = malloc(sizeof(struct vector) * recycle->maxvtx);
	recycle->tex = malloc(sizeof(struct texcol) * recycle->maxvtx);

	if (!recycle->triptr || !recycle->segptr || !recycle->pos || !recycle->nrm || !recycle->tex) {
		gx_debug("allocation failed");
		g3_destroyMesh(recycle);
		return NULL;
	}

	return recycle;
}
void g3_destroyMesh(gx_Mesh msh) {
	msh->maxtri = msh->tricnt = 0;
	msh->maxvtx = msh->vtxcnt = 0;
	free(msh->pos);
	msh->pos = 0;
	if (msh->nrm) {
		free(msh->nrm);
		msh->nrm = 0;
	}
	if (msh->tex) {
		free(msh->tex);
		msh->tex = 0;
	}
	if (msh->map && msh->freeMap) {
		msh->freeMap = 0;
		free(msh->map);
		msh->map = 0;
	}
	free(msh->triptr);
	free(msh->segptr);
	msh->triptr = 0;

	if (msh->freeMesh) {
		free(msh);
	}
}

/*long setgrp(gx_Mesh msh, long idx) {
	if (idx >= msh->maxgrp) {
		msh->maxgrp = HIBIT(idx) * 2;
		msh->grpptr = realloc(msh->pos, sizeof(struct grp*) * msh->maxgrp);
		if (!msh->grpptr) return -1;
	}
	if (msh->grpcnt <= idx) {
		msh->grpcnt = idx + 1;
	}
	return idx;
}*/
long getvtx(gx_Mesh msh, long idx) {
	if (idx >= msh->maxvtx) {
		msh->maxvtx = HIBIT(idx) * 2;
		msh->pos = (vector)realloc(msh->pos, sizeof(struct vector) * msh->maxvtx);
		msh->nrm = (vector)realloc(msh->nrm, sizeof(struct vector) * msh->maxvtx);
		msh->tex = (texcol)realloc(msh->tex, sizeof(struct texcol) * msh->maxvtx);
		if (!msh->pos || !msh->nrm || !msh->tex) return -1;
	}
	if (msh->vtxcnt <= idx) {
		msh->vtxcnt = idx + 1;
	}
	return idx;
}
long setvtx(gx_Mesh msh, long idx, vector pos, vector nrm, scalar tex[2]) {
	if (getvtx(msh, idx) < 0) {
		return -1;
	}
	if (pos != NULL) {
		msh->pos[idx] = *pos;
	}
	if (nrm != NULL) {
		msh->nrm[idx] = *nrm;
	}
	if (tex != NULL) {
		msh->tex[idx].s = (uint16_t) (tex[0] * 65535);
		msh->tex[idx].t = (uint16_t) (tex[1] * 65535);
	}
	return idx;
}

long addseg(gx_Mesh msh, long p1, long p2) {
	if (p1 >= msh->vtxcnt || p2 >= msh->vtxcnt)
		return -1;

	if (msh->segcnt >= msh->maxseg) {
		if (msh->maxseg == 0) {
			msh->maxseg = 16;
		}
		msh->segptr = (struct seg*)realloc(msh->segptr, sizeof(struct seg) * (msh->maxseg <<= 1));
		if (!msh->segptr) return -2;
	}

	msh->segptr[msh->segcnt].p1 = p1;
	msh->segptr[msh->segcnt].p2 = p2;
	return msh->segcnt++;
}
long addtri(gx_Mesh msh, long p1, long p2, long p3) {
	if (p1 >= msh->vtxcnt || p2 >= msh->vtxcnt || p3 >= msh->vtxcnt) {
		gx_debug("addTri(%ld, %ld, %ld)", p1, p2, p3);
		return -1;
	}

	#define H 1e-10
	if (vecdst(msh->pos + p1, msh->pos + p2) < H)
		return 0;
	if (vecdst(msh->pos + p1, msh->pos + p3) < H)
		return 0;
	if (vecdst(msh->pos + p2, msh->pos + p3) < H)
		return 0;
	#undef H

	if (msh->tricnt >= msh->maxtri) {
		msh->triptr = (struct tri*)realloc(msh->triptr, sizeof(struct tri) * (msh->maxtri <<= 1));
		if (msh->triptr == NULL) {
			return -1;
		}
	}
	msh->triptr[msh->tricnt].i1 = p1;
	msh->triptr[msh->tricnt].i2 = p2;
	msh->triptr[msh->tricnt].i3 = p3;
	return msh->tricnt++;
}
long addquad(gx_Mesh msh, long p1, long p2, long p3, long p4) {
	if (addtri(msh, p1, p2, p3) != -1) {
		return addtri(msh, p3, p4, p1);
	}
	return -1;
}

static int vtxcmp(gx_Mesh msh, int i, int j, scalar tol) {
	scalar dif;
	dif = msh->pos[i].x - msh->pos[j].x;
	if (tol < (dif < 0 ? -dif : dif)) return 2;
	dif = msh->pos[i].y - msh->pos[j].y;
	if (tol < (dif < 0 ? -dif : dif)) return 2;
	dif = msh->pos[i].z - msh->pos[j].z;
	if (tol < (dif < 0 ? -dif : dif)) return 2;
	//~ return msh->tex[i].val != msh->tex[j].val;
	return !(msh->tex && msh->tex[i].val == msh->tex[j].val);
}

static int prgDefCB(float prec) { (void)prec; return 0; }
static float precent(int i, int n) { return 100. * i / n; }

struct growBuffer {
	char *ptr;
	int max;
	//~ int cnt;
	int esz;
};
static void* growBuff(struct growBuffer* buff, int idx) {
	int pos = idx * buff->esz;
	if (pos >= buff->max) {
		buff->max = pos << 1;
		//~ while (pos >= buff->max) buff->max <<= 1;
		//~ gx_debug("realloc(%x, %d, %d):%d", buff->ptr, buff->esz, buff->max, idx);
		buff->ptr = realloc(buff->ptr, buff->max);
	}
	return buff->ptr ? buff->ptr + pos : NULL;
}
static void freeBuff(struct growBuffer* buff) {
	free(buff->ptr);
	buff->ptr = 0;
	buff->max = 0;
}
static void initBuff(struct growBuffer* buff, int initsize, int elemsize) {
	buff->ptr = 0;
	buff->max = initsize;
	buff->esz = elemsize;
	growBuff(buff, initsize);
}

static char* freadstr(char buff[], int maxlen, FILE *fin) {
	char *ptr = buff;
	for ( ; maxlen > 0; --maxlen) {
		int chr = fgetc(fin);

		if (chr == -1)
			break;

		if (chr == 0)
			break;

		*ptr++ = chr;
	}
	*ptr = 0;
	return buff;
}

int g3_readObj(gx_Mesh msh, const char *file) {
	FILE *fin;

	char *ws = " \t";
	char buff[65536];
	int posi = 0, texi = 0;
	int nrmi = 0, line = 0;
	struct growBuffer nrmb;
	struct growBuffer texb;

	if (!(fin = fopen(file, "rb")))
		return -1;

	initBuff(&nrmb, 64, 3 * sizeof(float));
	initBuff(&texb, 64, 2 * sizeof(float));

	for ( ; ; ) {
		char *ptr = buff;

		line++;
		if (!fgets(buff, sizeof(buff), fin))
			break;

		if (feof(fin)) break;

		// remove line end character
		if ((ptr = strchr(buff, 13)))
			*ptr = 0;

		if ((ptr = strchr(buff, 10)))
			*ptr = 0;

		if (*buff == '#')				// Comment
			continue;
		if (*buff == '\0')				// Empty line
			continue;

		// Grouping:
		if (readKVP(buff, "g", NULL, ws)) continue;		// Group name
		if (readKVP(buff, "s", NULL, ws)) continue;		// Smoothing group
		if (readKVP(buff, "o", NULL, ws)) continue;		// Object name
		//~ if (readKVP(buff, "mg", NULL, ws)) continue;	// Merging group

		// Display/render attributes:
		if (readKVP(buff, "usemtl", NULL, ws)) continue;		// Material name
		if (readKVP(buff, "mtllib", NULL, ws)) continue;		// Material library
		//~ if (readKVP(buff, "bevel", NULL, ws)) continue;	// Bevel interpolation
		//~ if (readKVP(buff, "c_interp", NULL, ws)) continue;	// Color interpolation
		//~ if (readKVP(buff, "d_interp", NULL, ws)) continue;	// Dissolve interpolation
		//~ if (readKVP(buff, "lod", NULL, ws)) continue;		// Level of detail
		//~ if (readKVP(buff, "shadow_obj", NULL, ws)) continue;	// Shadow casting
		//~ if (readKVP(buff, "trace_obj", NULL, ws)) continue;	// Ray tracing
		//~ if (readKVP(buff, "ctech", NULL, ws)) continue;	// Curve approximation technique
		//~ if (readKVP(buff, "stech", NULL, ws)) continue;	// Surface approximation technique

		// Vertex data:
		if ((ptr = readKVP(buff, "v", NULL, ws))) {		// Geometric vertices
			struct vector vtx;
			sscanf(ptr, "%f%f%f", &vtx.x, &vtx.y, &vtx.z);
			setvtx(msh, posi, &vtx, NULL, NULL);
			posi += 1;
			continue;
		}
		if ((ptr = readKVP(buff, "vt", NULL, ws))) {	// Texture vertices
			float *vtx = growBuff(&texb, texi);
			if (!vtx) {gx_debug("memory"); break;}

			sscanf(ptr, "%f%f", vtx + 0, vtx + 1);
			if (vtx[0] < 0) vtx[0] = -vtx[0];
			if (vtx[1] < 0) vtx[1] = -vtx[1];
			if (vtx[0] > 1) vtx[0] = 0;
			if (vtx[1] > 1) vtx[1] = 0;

			if (vtx[0] < 0 || vtx[0] > 1 || vtx[1] < 0 || vtx[1] > 1) {
				break;
			}

			texi += 1;
			continue;
		}
		if ((ptr = readKVP(buff, "vn", NULL, ws))) {	// Vertex normals
			float *vtx = growBuff(&nrmb, nrmi);
			if (!vtx) {gx_debug("memory"); break;}

			sscanf(ptr, "%f%f%f", vtx + 0, vtx + 1, vtx + 2);
			nrmi += 1;
			continue;
		}
		//~ if (readKVP(buff, "vp", NULL, ws)) continue;		// Parameter space vertices

		// Elements:
		if ((ptr = readKVP(buff, "f", NULL, ws))) {			// Face
			int i, v[4], vn[4], vt[4];

			for (i = 0; *ptr != 0; i++) {

				while (*ptr && strchr(" \t", *ptr)) ptr++;		// skip white spaces
				if (*ptr == '\0') break;				// end of line

				if (i > 4) {
					fprintf(stderr, "%s:%d: face too complex: `%s`\n", file, line, buff);
					*ptr = 0;
					break;						// error
				}
				if (!strchr("+-0123456789", *ptr)) {	// error
					fprintf(stderr, "unsupported line: `%s`\n", ptr);
					break;
				}

				v[i] = vn[i] = vt[i] = 0;
				ptr = readI32(ptr, &v[i]);				// position index
				if (*ptr == '/') {						// texture index
					ptr += 1;
					if (*ptr && strchr("+-0123456789", *ptr)) {
						ptr = readI32(ptr, &vt[i]);
					}
				}
				if (*ptr == '/') {						// normal index
					ptr += 1;
					if (*ptr && strchr("+-0123456789", *ptr)) {
						ptr = readI32(ptr, &vn[i]);
					}
				}

				//~ gx_debug("face fragment: vtx(%d), nrm(%d), tex(%d) `%s`", v[i], vn[i], vt[i], ptr);

				if (v[i]) v[i] += v[i] < 0 ? posi : -1;
				if (vn[i]) vn[i] += vn[i] < 0 ? nrmi : -1;
				if (vt[i]) vt[i] += vt[i] < 0 ? texi : -1;

				if (v[i] > posi || v[i] < 0) break;
				if (vn[i] > nrmi || vn[i] < 0) break;
				//~ if (vt[i] > texi || vt[i] < 0) break;
				if (vt[i] > texi || vt[i] < 0) vt[i] = 0;

				if (texi) {
					setvtx(msh, v[i], NULL, NULL, growBuff(&texb, vt[i]));
				}
				if (nrmi) {
					setvtx(msh, v[i], NULL, growBuff(&nrmb, vn[i]), NULL);
				}
			}

			if (*ptr != '\0')
				break;

			if (i == 3 && addtri(msh, v[0], v[1], v[2]) < 0)
				break;

			if (i == 4 && addquad(msh, v[0], v[1], v[2], v[3]) < 0)
				break;

			continue;
		}

		if (readKVP(buff, "p", NULL, ws)) continue;			// Point
		if (readKVP(buff, "l", NULL, ws)) continue;			// Line
		//~ if (readKVP(buff, "curv", NULL, ws)) continue;		// Curve
		//~ if (readKVP(buff, "curv2", NULL, ws)) continue;		// 2D curve
		//~ if (readKVP(buff, "surf", NULL, ws)) continue;		// Surface

		// Connectivity between free-form surfaces:
		//~ if (readKVP(buff, "con", NULL, ws)) continue;		// Connect

		// Free-form curve/surface attributes:
		//~ if (readKVP(buff, "deg", NULL, ws)) continue;		// Degree
		//~ if (readKVP(buff, "bmat", NULL, ws)) continue;	// Basis matrix
		//~ if (readKVP(buff, "step", NULL, ws)) continue;	// Step size
		//~ if (readKVP(buff, "cstype", NULL, ws)) continue;	// Curve or surface type

		// Free-form curve/surface body statements:
		//~ if (readKVP(buff, "parm", NULL, ws)) continue;	// Parameter values
		//~ if (readKVP(buff, "trim", NULL, ws)) continue;	// Outer trimming loop
		//~ if (readKVP(buff, "hole", NULL, ws)) continue;	// Inner trimming loop
		//~ if (readKVP(buff, "scrv", NULL, ws)) continue;	// Special curve
		//~ if (readKVP(buff, "sp", NULL, ws)) continue;		// Special point
		//~ if (readKVP(buff, "end", NULL, ws)) continue;		// End statement
		fprintf(stderr, "unsupported line: `%s`\n", buff);
		break;
	}

	freeBuff(&texb);
	freeBuff(&nrmb);

	if (!feof(fin)) {
		fprintf(stderr, "%s:%d: pos(%d), nrm(%d), tex(%d) `%s`\n", file, line, posi, nrmi, texi, buff);
		fclose(fin);
		return -2;
	}

	msh->hasNrm = nrmi != 0;
	msh->hasTex = texi != 0;

	fclose(fin);
	return 0;
}
int g3_read3ds(gx_Mesh msh, const char *file) {
	FILE *fin;
	char buff[65536];

	unsigned short chunk_id;
	unsigned int chunk_len;
	unsigned short qty;

	int i, vtxi = 0;
	int posi = 0, texi = 0;
	//~ int mtli = 0, obji = 0;				// material, mesh index

	if (!(fin = fopen (file, "rb"))) return -1;

	for ( ; ; ) {
		if (!fread(&chunk_id, 2, 1, fin)) //Read the chunk header
			break;
		if (!fread(&chunk_len, 4, 1, fin)) //Read the lenght of the chunk
			break;
		if (feof(fin))
			break;
		switch (chunk_id) {
			//#{ Color chunks
				//~ 0x0010 : Rgb (float)
				//~ 0x0011 : Rgb (byte)
				//~ 0x0012 : Rgb (byte) gamma corrected
				//~ 0x0013 : Rgb (float) gamma corrected
			//#}
			//#{ Percent chunks
				//~ 0x0030 : percent (int)
				//~ 0x0031 : percent (float)
			//#}
			//#{ 0x4D4D: Main chunk
				//~ 0x0002 : 3DS-Version
				//#{ 0x3D3D : 3D editor chunk
					//~ 0x0100 : One unit
					//~ 0x1100 : Background bitmap
					//~ 0x1101 : Use background bitmap
					//~ 0x1200 : Background color
					//~ 0x1201 : Use background color
					//~ 0x1300 : Gradient colors
					//~ 0x1301 : Use gradient
					//~ 0x1400 : Shadow map bias
					//~ 0x1420 : Shadow map size
					//~ 0x1450 : Shadow map sample range
					//~ 0x1460 : Raytrace bias
					//~ 0x1470 : Raytrace on
					//~ 0x2100 : Ambient color
					//~ 0x2200 : Fog
					//~ 0x2201 : Use fog
					//~ 0x2210 : Fog background
					//~ 0x2300 : Distance queue
						//~ 0x2310 : Dim background
					//~ 0x2301 : Use distance queue
					//~ 0x2302 : Layered fog options
					//~ 0x2303 : Use layered fog
					//~ 0x3D3E : Mesh version
					//#{ 0x4000 : Object block
						//~ 0x4010 : Object hidden
						//~ 0x4012 : Object doesn't cast
						//~ 0x4013 : Matte object
						//~ 0x4015 : External process on
						//~ 0x4017 : Object doesn't receive shadows
						//#{ 0x4100 : Triangular mesh
							//~ 0x4110 : Vertices list
							//~ 0x4120 : Faces description
								//~ 0x4130 : Faces material list
							//~ 0x4140 : Mapping coordinates list
								//~ 0x4150 : Smoothing group list
							//~ 0x4160 : Local coordinate system
							//~ 0x4165 : Object color in editor
							//~ 0x4181 : External process name
							//~ 0x4182 : External process parameters
						//#}
						//#{ 0x4600 : Light
							//#{ 0x4610 : Spotlight
								//~ 0x4627 : Spot raytrace
								//~ 0x4630 : Light shadowed
								//~ 0x4641 : Spot shadow map
								//~ 0x4650 : Spot show cone
								//~ 0x4651 : Spot is rectangular
								//~ 0x4652 : Spot overshoot
								//~ 0x4653 : Spot map
								//~ 0x4656 : Spot roll
								//~ 0x4658 : Spot ray trace bias
							//#}
							//~ 0x4620 : Light off
							//~ 0x4625 : Attenuation on
							//~ 0x4659 : Range start
							//~ 0x465A : Range end
							//~ 0x465B : Multiplier
						//#}
						//~ 0x4700 : Camera
					//#}
					//#{ 0x7001 : Window settings
						//~ 0x7011 : Window description #2 ...
						//~ 0x7012 : Window description #1 ...
						//~ 0x7020 : Mesh windows ...
					//#}
					//#{ 0xAFFF : Material block
						//~ 0xA000 : Material name
						//~ 0xA010 : Ambient color
						//~ 0xA020 : Diffuse color
						//~ 0xA030 : Specular color
						//~ 0xA040 : Shininess percent
						//~ 0xA041 : Shininess strength percent
						//~ 0xA050 : Transparency percent
						//~ 0xA052 : Transparency falloff percent
						//~ 0xA053 : Reflection blur percent
						//~ 0xA081 : 2 sided
						//~ 0xA083 : Add trans
						//~ 0xA084 : Self illum
						//~ 0xA085 : Wire frame on
						//~ 0xA087 : Wire thickness
						//~ 0xA088 : Face map
						//~ 0xA08A : In tranc
						//~ 0xA08C : Soften
						//~ 0xA08E : Wire in units
						//~ 0xA100 : Render type
						//~ 0xA240 : Transparency falloff percent present
						//~ 0xA250 : Reflection blur percent present
						//~ 0xA252 : Bump map present (true percent)
						//~ 0xA200 : Texture map 1
						//~ 0xA33A : Texture map 2
						//~ 0xA210 : Opacity map
						//~ 0xA230 : Bump map
						//~ 0xA33C : Shininess map
						//~ 0xA204 : Specular map
						//~ 0xA33D : Self illum. map
						//~ 0xA220 : Reflection map
						//~ 0xA33E : Mask for texture map 1
						//~ 0xA340 : Mask for texture map 2
						//~ 0xA342 : Mask for opacity map
						//~ 0xA344 : Mask for bump map
						//~ 0xA346 : Mask for shininess map
						//~ 0xA348 : Mask for specular map
						//~ 0xA34A : Mask for self illum. map
						//~ 0xA34C : Mask for reflection map
						//#{ Sub-chunks for all maps:
							//~ 0xA300 : Mapping filename
							//~ 0xA351 : Mapping parameters
							//~ 0xA353 : Blur percent
							//~ 0xA354 : V scale
							//~ 0xA356 : U scale
							//~ 0xA358 : U offset
							//~ 0xA35A : V offset
							//~ 0xA35C : Rotation angle
							//~ 0xA360 : RGB Luma/Alpha tint 1
							//~ 0xA362 : RGB Luma/Alpha tint 2
							//~ 0xA364 : RGB tint R
							//~ 0xA366 : RGB tint G
							//~ 0xA368 : RGB tint B
						//#}
					//#}
				//#}
				//#{ 0xB000 : Keyframer chunk
					//~ 0xB001 : Ambient light information block
					//~ 0xB002 : Mesh information block
					//~ 0xB003 : Camera information block
					//~ 0xB004 : Camera target information block
					//~ 0xB005 : Omni light information block
					//~ 0xB006 : Spot light target information block
					//~ 0xB007 : Spot light information block
					//#{ 0xB008 : Frames (Start and End)
						//~ 0xB010 : Object name, parameters and hierarchy father
						//~ 0xB013 : Object pivot point
						//~ 0xB015 : Object morph angle
						//~ 0xB020 : Position track
						//~ 0xB021 : Rotation track
						//~ 0xB022 : Scale track
						//~ 0xB023 : FOV track
						//~ 0xB024 : Roll track
						//~ 0xB025 : Color track
						//~ 0xB026 : Morph track
						//~ 0xB027 : Hotspot track
						//~ 0xB028 : Falloff track
						//~ 0xB029 : Hide track
						//~ 0xB030 : Hierarchy position
					//#}
				//#}
			//#} Main chunk
			case 0x4d4d: break;//gx_debug("Main chunk"); break;
				case 0x3d3d: break;//gx_debug("3D editor chunk"); break;
					//~ case 0x0100: // One unit
					//~ case 0x1100: // Background bitmap
					//~ case 0x1101: // Use background bitmap
					//~ case 0x1200: // Background color
					//~ case 0x1201: // Use background color
					//~ case 0x1300: // Gradient colors
					//~ case 0x1301: // Use gradient
					//~ case 0x1400: // Shadow map bias
					//~ case 0x1420: // Shadow map size
					//~ case 0x1450: // Shadow map sample range
					//~ case 0x1460: // Raytrace bias
					//~ case 0x1470: // Raytrace on
					//~ case 0x2100: // Ambient color
					//~ case 0x2200: // Fog
					//~ case 0x2201: // Use fog
					//~ case 0x2210: // Fog background
					//~ case 0x2300: // Distance queue
					//~ case 0x2310: // Dim background
					//~ case 0x2301: // Use distance queue
					//~ case 0x2302: // Layered fog options
					//~ case 0x2303: // Use layered fog
					//~ case 0x3D3E: // Mesh version
						//~ goto skipchunk;// */

					case 0x0002: goto skipchunk;		// 3DS-Version
					case 0xAFFF: goto skipchunk;		// Material block

					case 0x4000: {		// Object block
						char *objName = freadstr(buff, sizeof(buff), fin);
						vtxi = posi;
						//objName = objName;
						(void)objName;
						//~ gx_debug("Object: '%s'", objName);
					} break;
					case 0x4100: break;	// Triangular mesh
					case 0x4110: {		// Vertices list
						struct vector dat;
						if (fread(&qty, sizeof(qty), 1, fin) != 1)
							goto invChunk;
						for (i = 0; i < qty; i++) {
							if (fread(&dat, sizeof(float), 3, fin) != 3)
								goto invChunk;
							dat.w = 0;
							if (setvtx(msh, vtxi + i, &dat, NULL, NULL) < 0) return 1;
							//~ gx_debug("pos[%d](%f, %f, %f)", vtxi + i, dat[0], dat[1], dat[2]);
						}
						posi += qty;
					} break;
					case 0x4120: {		// Faces description
							unsigned short dat[4];
							if (fread (&qty, sizeof(qty), 1, fin) != 1)
								goto invChunk;
							for (i = 0; i < qty; i++) {
								if (fread(&dat, sizeof(unsigned short), 4, fin) != 4)
									goto invChunk;
								addtri(msh, vtxi + dat[0], vtxi + dat[1], vtxi + dat[2]);
							}
							//~ gx_debug("faces: %d, %d, %d", vtxi, posi, texi);
					} break;
					case 0x4130:		// Faces material list
						goto skipchunk;
					case 0x4140: {		// Mapping coordinates list
						float dat[3];
						if (fread (&qty, sizeof(qty), 1, fin) != 1)
							goto invChunk;
						for (i = 0; i < qty; i++) {
							if (fread(dat, sizeof(float), 2, fin) != 2)
								goto invChunk;
							dat[1] = 1 - dat[1];
							if (setvtx(msh, vtxi + i, NULL, NULL, dat) < 0) return 2;
						}
						texi += qty;
					} break;
					case 0x4150:		// Smoothing group list
					case 0x4160:		// Local coordinate system
					case 0x4165:		// Object color in editor
					case 0x4181:		// External process name
					case 0x4182:		// External process parameters
				case 0xB000:			// Keyframer chunk
					goto skipchunk;
			invChunk:
			default:
				gx_debug("unknown Chunk: ID: 0x%04x; Len: %d", chunk_id, chunk_len);
			skipchunk:
				fseek(fin, chunk_len - 6, SEEK_CUR);
				break;
		}
	}

	fclose (fin);
	msh->hasNrm = 1;
	normMesh(msh, 0, NULL, NULL);
	msh->hasTex = texi != 0;

	return 0;
}
int g3_saveObj(gx_Mesh msh, const char *file) {
	FILE* out;
	long i;

	if (!(out = fopen(file, "wt"))) {
		gx_debug("fopen(%s, 'wt')", file);
		return -1;
	}

	for (i = 0; i < msh->vtxcnt; ++i) {
		vector pos = msh->pos + i;
		vector nrm = msh->nrm + i;
		scalar s = msh->tex[i].s / 65535.;
		scalar t = msh->tex[i].t / 65535.;
		if (msh->hasTex && msh->hasNrm) {
			fprintf(out, "#vertex: %ld\n", i);
		}
		//~ fprintf(out, "v  %.*g %.*g %.*g\n", prec, pos->x, prec, pos->y, prec, pos->z);
		//~ fprintf(out, "v  %g %g %g\n", roundto(pos->x, prec), roundto(pos->y, prec), roundto(pos->z, prec));
		fprintf(out, "v  %g %g %g\n", pos->x, pos->y, pos->z);
		if (msh->hasTex) fprintf(out, "vt %f %f\n", s, -t);
		if (msh->hasNrm) fprintf(out, "vn %f %f %f\n", nrm->x, nrm->y, nrm->z);
	}

	for (i = 0; i < msh->tricnt; ++i) {
		int i1 = msh->triptr[i].i1 + 1;
		int i2 = msh->triptr[i].i2 + 1;
		int i3 = msh->triptr[i].i3 + 1;
		if (i + 1 < msh->tricnt) {
			int I1 = msh->triptr[i+1].i1 + 1;
			int i4 = msh->triptr[i+1].i2 + 1;
			int I3 = msh->triptr[i+1].i3 + 1;
			if (i1 == I3 && i3 == I1) {
				if (msh->hasTex && msh->hasNrm) fprintf(out, "f  %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d\n", i1, i1, i1, i2, i2, i2, i3, i3, i3, i4, i4, i4);
				else if (msh->hasNrm) fprintf(out, "f  %d//%d %d//%d %d//%d %d//%d\n", i1, i1, i2, i2, i3, i3, i4, i4);
				else if (msh->hasTex) fprintf(out, "f  %d/%d %d/%d %d/%d %d/%d\n", i1, i1, i2, i2, i3, i3, i4, i4);
				else fprintf(out, "f  %d %d %d %d\n", i1, i2, i3, i4);
				++i;
				continue;
			}
		}
		if (msh->hasTex && msh->hasNrm) fprintf(out, "f  %d/%d/%d %d/%d/%d %d/%d/%d\n", i1, i1, i1, i2, i2, i2, i3, i3, i3);
		else if (msh->hasNrm) fprintf(out, "f  %d//%d %d//%d %d//%d\n", i1, i1, i2, i2, i3, i3);
		else if (msh->hasTex) fprintf(out, "f  %d/%d %d/%d %d/%d\n", i1, i1, i2, i2, i3, i3);
		else fprintf(out, "f  %d %d %d\n", i1, i2, i3);
	}

	for (i = 0; i < msh->segcnt; ++i) {
		int i1 = msh->segptr[i].p1 + 1;
		int i2 = msh->segptr[i].p2 + 1;
		fprintf(out, "l %d %d\n", i1, i2);
	}

	fclose(out);

	return 0;
}

void bboxMesh(gx_Mesh msh, vector min, vector max) {
	long i;
	if (msh->vtxcnt == 0) {
		vecldf(min, 0, 0, 0, 0);
		vecldf(max, 0, 0, 0, 0);
		return;
	}

	*min = *max = msh->pos[0];

	for (i = 1; i < msh->vtxcnt; i += 1) {
		vecmax(max, max, &msh->pos[i]);
		vecmin(min, min, &msh->pos[i]);
	}
}

/*
void centMesh(gx_Mesh msh, scalar size) {
	long i;
	struct vector min, max, use;

	bboxMesh(msh, &min, &max);

	vecsca(&use, vecadd(&use, &max, &min), 1./2);		// scale

	for (i = 0; i < msh->vtxcnt; i += 1) {
		vecsub(&msh->pos[i], &msh->pos[i], &use);
	}

	if (size == 0) {
		return;
	}

	vecsub(&use, &max, &min);		// resize
	if (use.x < use.y)
		use.x = use.y;
	if (use.x < use.z)
		use.x = use.z;
	size = 2 * size / use.x;

	for (i = 0; i < msh->vtxcnt; i += 1) {
		vecsca(&msh->pos[i], &msh->pos[i], size);
		msh->pos[i].w = 1;
	}
}
*/

/** recalculate mesh normals
 * 
 */
void normMesh(gx_Mesh msh, scalar tolerance, vector recenter, vector resize) {
	struct vector move = vec(0, 0, 0, 0);
	struct vector size = vec(1, 1, 1, 0);
	struct vector min = vec(0, 0, 0, 0);
	struct vector max = vec(0, 0, 0, 0);

	//bboxMesh(msh, &min, &max);
	for (int i = 0; i < msh->vtxcnt; i += 1) {
		vecldf(&msh->nrm[i], 0, 0, 0, 1);
		vecmax(&max, &max, &msh->pos[i]);
		vecmin(&min, &min, &msh->pos[i]);
	}

	for (int i = 0; i < msh->tricnt; i += 1) {
		struct vector nrm;
		long i1 = msh->triptr[i].i1;
		long i2 = msh->triptr[i].i2;
		long i3 = msh->triptr[i].i3;
		backface(&nrm, &msh->pos[i1], &msh->pos[i2], &msh->pos[i3]);
		vecadd(&msh->nrm[i1], &msh->nrm[i1], &nrm);
		vecadd(&msh->nrm[i2], &msh->nrm[i2], &nrm);
		vecadd(&msh->nrm[i3], &msh->nrm[i3], &nrm);
	}

	if (tolerance != 0) {
		for (int i = 1; i < msh->vtxcnt; i += 1) {
			for (int j = 0; j < i; j += 1) {
				if (vtxcmp(msh, i, j, tolerance) < 2) {
					struct vector nrm;
					vecadd(&nrm, &msh->nrm[j], &msh->nrm[i]);
					msh->nrm[i] = nrm;
					msh->nrm[j] = nrm;
					break;
				}
			}
		}
	}

	if (recenter != NULL) {
		vecadd(&move, &max, &min);
		vecsca(&move, &move, 1.f / 2);
		vecadd(&move, &move, recenter);
	}

	if (resize != NULL) {
		vecsub(&size, &max, &min);
		scalar asp = 0;
		if (asp < size.x) {
			asp = size.x;
		}
		if (asp < size.y) {
			asp = size.y;
		}
		if (asp < size.z) {
			asp = size.z;
		}

		if (resize->x != 0) {
			size.x = resize->x / asp;
		}
		if (resize->y != 0) {
			size.y = resize->y / asp;
		}
		if (resize->z != 0) {
			size.z = resize->z / asp;
		}
	}

	// normalize normals, move 
	for (int i = 0; i < msh->vtxcnt; i += 1) {
		vecsub(&msh->pos[i], &msh->pos[i], &move);
		vecmul(&msh->pos[i], &msh->pos[i], &size);
		msh->pos[i].w = 1;
		vecnrm(&msh->nrm[i], &msh->nrm[i]);
		msh->nrm[i].w = 0;
	}
}

/**	subdivide mesh
 *	TODO: not bezier curve => patch eval
**/
static void pntri(struct vector res[10], gx_Mesh msh, int i1, int i2, int i3) {
	#define P1 (msh->pos + i1)
	#define P2 (msh->pos + i2)
	#define P3 (msh->pos + i3)
	#define N1 (msh->nrm + i1)
	#define N2 (msh->nrm + i2)
	#define N3 (msh->nrm + i3)
	struct vector tmp[3];
	scalar w12 = vecdp3(vecsub(tmp, P2, P1), N1);
	scalar w13 = vecdp3(vecsub(tmp, P3, P1), N1);
	scalar w21 = vecdp3(vecsub(tmp, P1, P2), N2);
	scalar w23 = vecdp3(vecsub(tmp, P3, P2), N2);
	scalar w31 = vecdp3(vecsub(tmp, P1, P3), N3);
	scalar w32 = vecdp3(vecsub(tmp, P2, P3), N3);

	vector b300 = veccpy(res + 0, P1);
	vector b030 = veccpy(res + 1, P2);
	vector b003 = veccpy(res + 2, P3);

	vector b210 = vecsca(res + 3, vecsub(tmp, vecadd(tmp, vecsca(tmp, P1, 2), P2), vecsca(tmp + 1, N1, w12)), 1./3);
	vector b120 = vecsca(res + 4, vecsub(tmp, vecadd(tmp, vecsca(tmp, P2, 2), P1), vecsca(tmp + 1, N2, w21)), 1./3);
	vector b021 = vecsca(res + 5, vecsub(tmp, vecadd(tmp, vecsca(tmp, P2, 2), P3), vecsca(tmp + 1, N2, w23)), 1./3);
	vector b012 = vecsca(res + 6, vecsub(tmp, vecadd(tmp, vecsca(tmp, P3, 2), P2), vecsca(tmp + 1, N3, w32)), 1./3);
	vector b102 = vecsca(res + 7, vecsub(tmp, vecadd(tmp, vecsca(tmp, P3, 2), P1), vecsca(tmp + 1, N3, w31)), 1./3);
	vector b201 = vecsca(res + 8, vecsub(tmp, vecadd(tmp, vecsca(tmp, P1, 2), P3), vecsca(tmp + 1, N1, w13)), 1./3);

	vector E = vecsca(tmp + 1, vecadd(tmp, vecadd(tmp, vecadd(tmp, vecadd(tmp, vecadd(tmp, b210, b120), b021), b012), b102), b201), 1./6);
	vector V = vecsca(tmp + 2, vecadd(tmp, vecadd(tmp, P1, P2), P3), 1./3);
	vector b111 = vecsca(res + 9, vecadd(tmp, E, vecsub(tmp, E, V)), 1./2);

	(void)b300;
	(void)b030;
	(void)b003;

	(void)b210;
	(void)b120;
	(void)b021;
	(void)b012;
	(void)b102;
	(void)b201;

	(void)b111;

	#undef P1
	#undef P2
	#undef P3
	#undef N1
	#undef N2
	#undef N3
}

static vector bezexp(struct vector res[4], vector p0, vector c0, vector c1, vector p1) {
	struct vector tmp[5];
	p0 = veccpy(tmp + 1, p0);
	c0 = veccpy(tmp + 2, c0);
	p1 = veccpy(tmp + 3, p1);
	c1 = veccpy(tmp + 4, c1);

	veccpy(res + 0, p0);
	vecsca(res + 1, vecsub(tmp, c0, p0), 3);
	vecsub(res + 2, vecsca(tmp, vecsub(tmp, c1, c0), 3), res+1);
	vecsub(res + 3, vecsub(tmp, vecsub(tmp, p1, res+2), res+1), res+0);
	return res;
}
static vector bezevl(struct vector res[1], struct vector p[4], scalar t) {
	//~ p = (((p3 * t + p2) * t + p1) * t + p0);
	vecadd(res, vecsca(res, p+3, t), p+2);
	vecadd(res, vecsca(res, res, t), p+1);
	vecadd(res, vecsca(res, res, t), p+0);
	return res;
}

// subdivide using bezier
int sdvtxBP(gx_Mesh msh, int a, int b, vector c1, vector c2) {
	struct vector tmp[6];//, res[3];
	int r = getvtx(msh, msh->vtxcnt);
	vecnrm(msh->nrm + r, vecadd(tmp, msh->nrm + a, msh->nrm + b));
	bezevl(msh->pos + r, bezexp(tmp, msh->pos + a, c1, c2, msh->pos + b), .5);
	return r;
}
int sdvtx(gx_Mesh msh, int a, int b) {
	struct vector tmp[1];
	int r = getvtx(msh, msh->vtxcnt);
	vecnrm(msh->nrm + r, vecadd(tmp, msh->nrm + a, msh->nrm + b));
	vecsca(msh->pos + r, vecadd(tmp, msh->pos + a, msh->pos + b), .5);
	return r;
}

void sdivMesh(gx_Mesh msh, int smooth) {
	unsigned i, n = msh->tricnt;
	if (smooth) for (i = 0; i < n; i++) {
		struct vector pn[11];
		unsigned i1 = msh->triptr[i].i1;
		unsigned i2 = msh->triptr[i].i2;
		unsigned i3 = msh->triptr[i].i3;
		unsigned i4, i5, i6;
		pntri(pn, msh, i1, i2, i3);
		i4 = sdvtxBP(msh, i1, i2, pn + 3, pn + 4);
		i5 = sdvtxBP(msh, i2, i3, pn + 5, pn + 6);
		i6 = sdvtxBP(msh, i3, i1, pn + 7, pn + 8);

		msh->triptr[i].i2 = i4;
		msh->triptr[i].i3 = i6;
		addtri(msh, i4, i2, i5);
		addtri(msh, i5, i3, i6);
		addtri(msh, i4, i5, i6);
	}
	else for (i = 0; i < n; i++) {
		signed i1 = msh->triptr[i].i1;
		signed i2 = msh->triptr[i].i2;
		signed i3 = msh->triptr[i].i3;
		signed i4 = sdvtx(msh, i1, i2);
		signed i5 = sdvtx(msh, i2, i3);
		signed i6 = sdvtx(msh, i3, i1);

		msh->triptr[i].i2 = i4;
		msh->triptr[i].i3 = i6;
		addtri(msh, i4, i2, i5);
		addtri(msh, i5, i3, i6);
		addtri(msh, i4, i5, i6);
	}
}
// */

void optiMesh(gx_Mesh msh, scalar tol, int prgCB(float prec)) {
	long i, j, k, n;
	if (prgCB == NULL)
		prgCB = prgDefCB;
	prgCB(0);
	for (n = i = 1; i < msh->vtxcnt; i += 1) {
		if (prgCB(precent(i, msh->vtxcnt)) == -1) break;
		for (j = 0; j < n && vtxcmp(msh, i, j, tol) != 0; j += 1);

		if (j == n) {
			msh->pos[n] = msh->pos[i];
			msh->nrm[n] = msh->nrm[i];
			msh->tex[n] = msh->tex[i];
			n++;
		}
		//~ else vecadd(&msh->vtxptr[j].nrm, &msh->vtxptr[j].nrm, &msh->vtxptr[i].nrm);
		for (k = 0; k < msh->tricnt; k += 1) {
			if (msh->triptr[k].i1 == i) msh->triptr[k].i1 = j;
			if (msh->triptr[k].i2 == i) msh->triptr[k].i2 = j;
			if (msh->triptr[k].i3 == i) msh->triptr[k].i3 = j;
		}
	}
	msh->vtxcnt = n;
	if (prgCB(100) == -1) return;
	//~ printf("vtx cnt %d\n", msh->vtxcnt);

	//~ for (j = 0; j < n; j += 1) vecnrm(&msh->vtxptr[j].nrm, &msh->vtxptr[j].nrm);
}// */
