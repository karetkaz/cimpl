/*******************************************************************************
 *   File: openGl.c
 *   Date: 2018.01.24
 *   Desc: openGl wrapper
 *******************************************************************************
gcc -shared -fPIC -Wall -g0 -Os -o out/libOpenGL.so src/plugin/openGL.c -I src/ -lGL -lGLU -lglut
 */

#include "cmpl.h"
#include <GL/glut.h>
#include <cmpl.h>

static inline size_t nextArg(nfcContext ctx) {
	return ctx->rt->api.nfcNextArg(ctx);
}

static inline symn argSym(nfcContext ctx, size_t offset) {
	return ctx->rt->api.rtLookup(ctx->rt, argref(ctx, offset));
}
static inline void *argPtr(nfcContext ctx, size_t offset) {
	return ctx->rt->_mem + argref(ctx, offset);
}
static inline rtValue argVal(nfcContext ctx, size_t offset) {
	return ctx->rt->api.nfcReadArg(ctx, offset);
}


static inline symn ccBegin(rtContext rt, const char *name) {
	return rt->api.ccBegin(rt->cc, name);
}
static inline symn ccEnd(rtContext rt, symn cls) {
	return rt->api.ccEnd(rt->cc, cls);
}

//#{ gl wrapper functions

static vmError glFun_GetString(nfcContext ctx) {
	uint32_t name = argu32(ctx, nextArg(ctx));
	rtValue buffer = argVal(ctx, nextArg(ctx));
	char *src = (char *)glGetString(name);
	char *dst = buffer.ref;

	if (src != NULL && dst != NULL && buffer.length > 0) {
		char *end = dst + buffer.length - 1;
		while (dst < end) {
			if (!(*dst = *src)) {
				break;
			}
			src++;
			dst++;
		}
		*dst = 0;
	}
	retu32(ctx, dst - (char *)buffer.ref);
	return noError;
}
static vmError glFun_Viewport(nfcContext ctx) {
	int x = argi32(ctx, nextArg(ctx));
	int y = argi32(ctx, nextArg(ctx));
	int w = argi32(ctx, nextArg(ctx));
	int h = argi32(ctx, nextArg(ctx));
	glViewport(x, y, w, h);
	return noError;
}
static vmError glFun_ShadeModel(nfcContext ctx) {
	glShadeModel(argu32(ctx, 0));
	return noError;
}
static vmError glFun_CullFace(nfcContext ctx) {
	glCullFace(argu32(ctx, 0));
	return noError;
}
static vmError glFun_Enable(nfcContext ctx) {
	glEnable(argu32(ctx, 0));
	return noError;
}
static vmError glFun_Disable(nfcContext ctx) {
	glDisable(argu32(ctx, 0));
	return noError;
}
static vmError glFun_Clear(nfcContext ctx) {
	glClear(argu32(ctx, 0));
	return noError;
}
static vmError glFun_Flush(nfcContext ctx) {
	glFlush();
	return noError;
}

static vmError glFun_Begin(nfcContext ctx) {
	glBegin(argu32(ctx, 0));
	return noError;
}
static vmError glFun_End(nfcContext ctx) {
	glEnd();
	return noError;
}

static vmError glFun_Vertex(nfcContext ctx) {
	float x = argf32(ctx, nextArg(ctx));
	float y = argf32(ctx, nextArg(ctx));
	float z = argf32(ctx, nextArg(ctx));
	float w = argf32(ctx, nextArg(ctx));
	glVertex4f(x, y, z, w);
	return noError;
}
static vmError glFun_Normal(nfcContext ctx) {
	float x = argf32(ctx, nextArg(ctx));
	float y = argf32(ctx, nextArg(ctx));
	float z = argf32(ctx, nextArg(ctx));
	//~ float w = argf32(ctx, nextArg(ctx));	// we have this value on the stack, but we don't need it.
	glNormal3f(x, y, z);
	return noError;
}
static vmError glFun_Color(nfcContext ctx) {
	float r = argf32(ctx, nextArg(ctx));
	float g = argf32(ctx, nextArg(ctx));
	float b = argf32(ctx, nextArg(ctx));
	float a = argf32(ctx, nextArg(ctx));
	glColor4f(r, g, b, a);
	return noError;
}

// matrix operations
static vmError glFun_MatrixMode(nfcContext ctx) {
	glMatrixMode(argu32(ctx, 0));
	return noError;
}

static vmError glFun_Frustum(nfcContext ctx) {
	GLdouble left   = argf64(ctx, nextArg(ctx));
	GLdouble right  = argf64(ctx, nextArg(ctx));
	GLdouble bottom = argf64(ctx, nextArg(ctx));
	GLdouble top    = argf64(ctx, nextArg(ctx));
	GLdouble near  = argf64(ctx, nextArg(ctx));
	GLdouble far   = argf64(ctx, nextArg(ctx));
	glFrustum(left, right, bottom, top, near, far);
	return noError;
}
static vmError glFun_Ortho(nfcContext ctx) {
	GLdouble left   = argf64(ctx, nextArg(ctx));
	GLdouble right  = argf64(ctx, nextArg(ctx));
	GLdouble bottom = argf64(ctx, nextArg(ctx));
	GLdouble top    = argf64(ctx, nextArg(ctx));
	GLdouble near  = argf64(ctx, nextArg(ctx));
	GLdouble far   = argf64(ctx, nextArg(ctx));
	glOrtho(left, right, bottom, top, near, far);
	return noError;
}

static vmError glFun_LoadMatrix(nfcContext ctx) {
	double *ptr = argPtr(ctx, 0);
	glLoadMatrixd(ptr);
	return noError;
}
static vmError glFun_MultMatrix(nfcContext ctx) {
	double *ptr = argPtr(ctx, 0);
	glMultMatrixd(ptr);
	return noError;
}
static vmError glFun_LoadIdentity(nfcContext ctx) {
	glLoadIdentity();
	return noError;
}
static vmError glFun_PushMatrix(nfcContext ctx) {
	glPushMatrix();
	return noError;
}
static vmError glFun_PopMatrix(nfcContext ctx) {
	glPopMatrix();
	return noError;
}

static vmError glFun_Rotate(nfcContext ctx) {
	GLdouble a = argf64(ctx, nextArg(ctx));
	GLdouble x = argf64(ctx, nextArg(ctx));
	GLdouble y = argf64(ctx, nextArg(ctx));
	GLdouble z = argf64(ctx, nextArg(ctx));
	glRotated(a, x, y, z);
	return noError;
}
static vmError glFun_Scale(nfcContext ctx) {
	GLdouble x = argf64(ctx, nextArg(ctx));
	GLdouble y = argf64(ctx, nextArg(ctx));
	GLdouble z = argf64(ctx, nextArg(ctx));
	glScaled(x, y, z);
	return noError;
}
static vmError glFun_Translate(nfcContext ctx) {
	GLdouble x = argf64(ctx, nextArg(ctx));
	GLdouble y = argf64(ctx, nextArg(ctx));
	GLdouble z = argf64(ctx, nextArg(ctx));
	glTranslated(x, y, z);
	return noError;
}
//#} opengl api

//#{ glu wrapper functions
static vmError gluFun_LookAt(nfcContext ctx) {
	GLdouble atx = argf64(ctx, nextArg(ctx));
	GLdouble aty = argf64(ctx, nextArg(ctx));
	GLdouble atz = argf64(ctx, nextArg(ctx));
	GLdouble tox = argf64(ctx, nextArg(ctx));
	GLdouble toy = argf64(ctx, nextArg(ctx));
	GLdouble toz = argf64(ctx, nextArg(ctx));
	GLdouble upx = argf64(ctx, nextArg(ctx));
	GLdouble upy = argf64(ctx, nextArg(ctx));
	GLdouble upz = argf64(ctx, nextArg(ctx));

	gluLookAt(atx, aty, atz, tox, toy, toz, upx, upy, upz);
	return noError;
}
//#}

//#{ glut handlers
static int btn = 0;

static rtContext rt = NULL;

// script side callback functions
static symn onMouse = NULL;
static symn onMotion = NULL;
static symn onKeyboard = NULL;
static symn onDisplay = NULL;
static symn onReshape = NULL;

static void mouse(int _btn, int state, int x, int y) {
	if (onMouse && rt) {
#pragma pack(push, 4)
		struct {int32_t y, x, btn;} args = {
			.btn = btn,
			.x = x,
			.y = y
		};
#pragma pack(pop)

		//~ _btn = 1 << _btn;
		_btn += 1;

		switch (state) {
			default:
				break;

			case GLUT_UP:
				btn &= ~_btn;
				break;

			case GLUT_DOWN:
				btn |= _btn;
				break;
		}

		rt->api.invoke(rt, onMouse, NULL, &args, NULL);
	}
}
static void motion(int x, int y) {
	if (onMotion && rt) {
#pragma pack(push, 4)
		struct {int32_t y, x, btn;} args = {
			.btn = btn,
			.x = x,
			.y = y
		};
#pragma pack(pop)
		rt->api.invoke(rt, onMotion, NULL, &args, NULL);
	}
}
static void keyboard(unsigned char key, int x, int y) {
	if (onKeyboard && rt) {
#pragma pack(push, 4)
		struct {int32_t key;} args = {key};
#pragma pack(pop)
		rt->api.invoke(rt, onKeyboard, NULL, &args, NULL);
	}
	//~ debugGL("keyboard(key:%d)", key);
}
static void display(void) {
	if (onDisplay && rt) {
		rt->api.invoke(rt, onDisplay, NULL, NULL, NULL);
	}
}
static void reshape(int x, int y) {
	if (onReshape && rt) {
#pragma pack(push, 4)
		struct {int y, x;} args = {
			.x = x,
			.y = y
		};
#pragma pack(pop)
		rt->api.invoke(rt, onReshape, NULL, &args, NULL);
	}
}
//#}

//#{ glut wrapper functions
static vmError glutFun_DisplayCB(nfcContext ctx) {
	onDisplay = argSym(ctx, 0);
	return noError;
}
static vmError glutFun_ReshapeCB(nfcContext ctx) {
	onReshape = argSym(ctx, 0);
	return noError;
}
static vmError glutFun_MouseCB(nfcContext ctx) {
	onMouse = argSym(ctx, 0);
	return noError;
}
static vmError glutFun_MotionCB(nfcContext ctx) {
	onMotion = argSym(ctx, 0);
	return noError;
}
static vmError glutFun_KeyboardCB(nfcContext ctx) {
	onKeyboard = argSym(ctx, 0);
	return noError;
}

static vmError glutFun_Reshape(nfcContext ctx) {
	int width = argi32(ctx, 0);
	int height = argi32(ctx, 4);
	glutReshapeWindow(width, height);
	return noError;
}

static vmError glutFun_PostRedisplay(nfcContext ctx) {
	glutPostRedisplay();
	return noError;
}
static vmError glutFun_SwapBuffers(nfcContext ctx) {
	glutSwapBuffers();
	return noError;
}

static vmError glutFun_FullScreen(nfcContext ctx) {
	glutFullScreen();
	return noError;
}

static vmError glutFun_MainLoop(nfcContext ctx) {
	int argc = 0;
	char *argv[] = {""};
	int width = argi32(ctx, nextArg(ctx));
	int height = argi32(ctx, nextArg(ctx));
	char *title = argPtr(ctx, nextArg(ctx));

	glutInit(&argc, argv);
	glutInitWindowSize(width, height);
	glutCreateWindow(title);

	glutKeyboardFunc(&keyboard);
	glutMouseFunc(&mouse);
	glutMotionFunc(&motion);
	glutDisplayFunc(&display);
	glutReshapeFunc(&reshape);

	glutMainLoop();
	return noError;
}
static vmError glutFun_ExitLoop(nfcContext ctx) {
	// FIXME: glutLeaveMainLoop();
	glutDestroyWindow(glutGetWindow());
	return noError;
}
static vmError glutFun_InitDisplayMode(nfcContext ctx) {
	uint32_t mode = argu32(ctx, 0);
	glutInitDisplayMode(mode);
	return noError;
}
//#} glut api

#ifdef _WIN32
__declspec( dllexport )
#endif
int cmplInit(rtContext _rt) {

	struct {
		vmError (*fun)(nfcContext);
		char *def;
	}
	libcGl[] = {
		{glFun_GetString,              "int getString(int value, char buffer[])"},
		{glFun_Viewport,               "void viewport(int x, int y, int width, int height)"},
		{glFun_Enable,                 "void enable(int32 mode)"},
		{glFun_Disable,                "void disable(int32 mode)"},
		{glFun_ShadeModel,             "void shadeModel(int mode)"},
		{glFun_CullFace,               "void cullFace(int mode)"},

		{glFun_Clear,                  "void clear(int32 Buffer)"},
		{glFun_Begin,                  "void begin(int32 Primitive)"},
		{glFun_End,                    "void end()"},
		{glFun_Flush,                  "void flush()"},

		{glFun_Vertex,                 "void vertex(float32 x, float32 y, float32 z, float32 w)"},
		{glFun_Normal,                 "void normal(float32 x, float32 y, float32 z)"},
		{glFun_Color,                  "void color(float32 r, float32 g, float32 b, float32 a)"},

		// Matrix operations
		{glFun_MatrixMode,             "void matrixMode(int mode)"},
		{glFun_LoadMatrix,             "void loadMatrix(double x[16])"},
		{glFun_MultMatrix,             "void multMatrix(double x[16])"},
		{glFun_LoadIdentity,           "void loadIdentity()"},
		{glFun_PushMatrix,             "void pushMatrix()"},
		{glFun_PopMatrix,              "void popMatrix()"},
		{glFun_Rotate,                 "void rotate(float64 angle, float64 x, float64 y, float64 z)"},
		{glFun_Scale,                  "void scale(float64 angle, float64 x, float64 y, float64 z)"},
		{glFun_Translate,              "void translate(float64 angle, float64 x, float64 y, float64 z)"},

		{glFun_Frustum,                "void frustum(float64 left, float64 right, float64 bottom, float64 top, float64 near, float64 far)"},
		{glFun_Ortho,                  "void ortho(float64 left, float64 right, float64 bottom, float64 top, float64 near, float64 far)"},
		//{glFun_,                     "void ()"},
	},
	libcGlu[] = {
		{gluFun_LookAt,                "void lookAt(float64 eyeX, float64 eyeY, float64 eyeZ, float64 centerX, float64 centerY, float64 centerZ, float64 upX, float64 upY, float64 upZ )"},
		//{gluFun_,                    "void ()"},
	},
	libcGlut[] = {
		// handlers
		{glutFun_DisplayCB,            "void onDisplay(void callback())"},
		{glutFun_ReshapeCB,            "void onReshape(void callback(int x, int y))"},
		{glutFun_MouseCB,              "void onMouse(void callback(int btn, int x, int y))"},
		{glutFun_MotionCB,             "void onMotion(void callback(int btn, int x, int y))"},
		{glutFun_KeyboardCB,           "void onKeyboard(void callback(int btn))"},

		{glutFun_PostRedisplay,        "void postRedisplay()"},
		{glutFun_Reshape,              "void reshape(int width, int height)"},
		{glutFun_MainLoop,             "void mainLoop(int width, int height, char title[*])"},
		{glutFun_ExitLoop,             "void exit()"},
		{glutFun_InitDisplayMode,      "void initDisplayMode(int mode)"},
		{glutFun_SwapBuffers,          "void swapBuffers()"},
		{glutFun_FullScreen,           "void fullScreen()"},
		//{glutFun_,                   "void ()"},
	};

	struct {
		size_t value;
		char *name;
	}
	defsGl[] = {
		// glGetString
		{GL_VENDOR,                    "Vendor"},
		{GL_RENDERER,                  "Renderer"},
		{GL_VERSION,                   "Version"},
		{GL_SHADING_LANGUAGE_VERSION,  "ShLVersion"},
		{GL_EXTENSIONS,                "Extensions"},

		// enable / disable
		{GL_DEPTH_TEST,                "DepthTest"},		// If enabled, do depth comparisons and update the depth buffer. Note that even if the depth buffer exists and the depth mask is non-zero, the depth buffer is not updated if the depth test is disabled. See glDepthFunc and
		{GL_LINE_SMOOTH,               "LineSmooth"},		// If enabled, draw lines with correct filtering. Otherwise, draw aliased lines. See glLineWidth.
		{GL_CULL_FACE,                 "CullFace"},			// 

		// CullFace
		{GL_FRONT,                     "Front"},
		{GL_BACK,                      "Back"}, 
		{GL_FRONT_AND_BACK,            "FrontAndBack"},

		// ShadeModel
		{GL_FLAT,                      "Flat"}, 
		{GL_SMOOTH,                    "Smooth"}, 

		// clear
		{GL_COLOR_BUFFER_BIT,          "ColorBuffer"},		// Indicates the buffers currently enabled for color writing.
		{GL_DEPTH_BUFFER_BIT,          "DepthBuffer"},		// Indicates the depth buffer.
		{GL_ACCUM_BUFFER_BIT,          "AccumBuffer"},		// Indicates the accumulation buffer.
		{GL_STENCIL_BUFFER_BIT,        "StencilBuffer"},	// Indicates the stencil buffer.

		// matrix mode
		{GL_MODELVIEW,                 "Modelview"},		// Applies subsequent matrix operations to the modelview matrix stack.
		{GL_PROJECTION,                "Projection"},		// Applies subsequent matrix operations to the projection matrix stack.
		{GL_TEXTURE,                   "Texture"},			// Applies subsequent matrix operations to the texture matrix stack.
		{GL_COLOR,                     "Color"},			// Applies subsequent matrix operations to the color matrix stack.

		// primitives
		{GL_POINTS,                    "Points"},			// Treats each vertex as a single point. Vertex n defines point n . N points are drawn.
		{GL_LINES,                     "Lines"},			// Treats each pair of vertices as an independent line segment. Vertices 2 n - 1 and 2 n define line n . N / 2 lines are drawn.
		{GL_LINE_STRIP,                "LineStrip"},		// Draws a connected group of line segments from the first vertex to the last. Vertices n and n + 1 define line n . N - 1 lines are drawn.
		{GL_LINE_LOOP,                 "LineLoop"},			// Draws a connected group of line segments from the first vertex to the last, then back to the first. Vertices n and n + 1 define line n . The last line, however, is defined by vertices N and 1 . N lines are drawn.
		{GL_TRIANGLES,                 "Triangles"},		// Treats each triplet of vertices as an independent triangle. Vertices 3 n - 2 , 3 n - 1 , and 3 n define triangle n . N / 3 triangles are drawn.
		{GL_TRIANGLE_STRIP,            "TriangleStrip"},	// Draws a connected group of triangles. One triangle is defined for each vertex presented after the first two vertices. For odd n , vertices n , n + 1 , and n + 2 define triangle n . For even n , vertices n + 1 , n , and n + 2 define triangle n . N - 2 triangles are drawn.
		{GL_TRIANGLE_FAN,              "TriangleFan"},		// Draws a connected group of triangles. One triangle is defined for each vertex presented after the first two vertices. Vertices 1 , n + 1 , and n + 2 define triangle n . N - 2 triangles are drawn.
		{GL_QUADS,                     "Quads"},			// Treats each group of four vertices as an independent quadrilateral. Vertices 4 n - 3 , 4 n - 2 , 4 n - 1 , and 4 n define quadrilateral n . N / 4 quadrilaterals are drawn.
		{GL_QUAD_STRIP,                "QuadStrip"},		// Draws a connected group of quadrilaterals. One quadrilateral is defined for each pair of vertices presented after the first pair. Vertices 2 n - 1 , 2 n , 2 n + 2 , and 2 n + 1 define quadrilateral n . N / 2 - 1 quadrilaterals are drawn. Note that the order in which vertices are used to construct a quadrilateral from strip data is different from that used with independent data.
		{GL_POLYGON,                   "Polygon"},			// Draws a single, convex polygon. Vertices 1 through N define this polygon.
	},
	defsGlu[] = {
		{0, NULL}
	},
	defsGlut[] = {
		// the display mode definitions
		{GLUT_RGB,                     "Rgb"},
		{GLUT_RGBA,                    "Rgba"},
		{GLUT_INDEX,                   "Index"},
		{GLUT_SINGLE,                  "Single"},
		{GLUT_DOUBLE,                  "Double"},
		{GLUT_ACCUM,                   "Accum"},
		{GLUT_ALPHA,                   "Alpha"},
		{GLUT_DEPTH,                   "Depth"},
		{GLUT_STENCIL,                 "Stencil"},
		{GLUT_MULTISAMPLE,             "Multisample"},
		{GLUT_STEREO,                  "Stereo"},
		{GLUT_LUMINANCE,               "Luminance"},
	};

	symn nsp;	// namespace

	rt = _rt;
	if ((nsp = ccBegin(rt, "gl")) != NULL) {

		for (int i = 0; i < sizeof(defsGl) / sizeof(*defsGl); i += 1) {
			if (!rt->api.ccDefInt(rt->cc, defsGl[i].name, defsGl[i].value)) {
				return +1;
			}
		}

		for (int i = 0; i < sizeof(libcGl) / sizeof(*libcGl); i += 1) {
			if (!rt->api.ccDefCall(rt->cc, libcGl[i].fun, libcGl[i].def)) {
				return -1;
			}
		}

		/* TODO: add some inline code
		if (!rt->api.ccDef???(rt, 0, __FILE__, __LINE__ + 1,
			"inline Vertex(float32 x, float32 y, float32 z) = Vertex(x, y, z, 1.);\n"
			"inline Color(float32 r, float32 g, float32 b) = Color(r, g, b, 1.);\n"
		)) {
			return -1;
		}*/
		ccEnd(rt, nsp);
	}
	if ((nsp = ccBegin(rt, "glu"))) {
		for (int i = 0; i < sizeof(defsGlu) / sizeof(*defsGlu); i += 1) {
			if (defsGlu[i].name == NULL) {
				continue;
			}
			if (!rt->api.ccDefInt(rt->cc, defsGlu[i].name, defsGlu[i].value)) {
				return -2;
			}
		}
		for (int i = 0; i < sizeof(libcGlu) / sizeof(*libcGlu); i += 1) {
			if (!rt->api.ccDefCall(rt->cc, libcGlu[i].fun, libcGlu[i].def)) {
				return -2;
			}
		}
		ccEnd(rt, nsp);
	}
	if ((nsp = ccBegin(rt, "glut"))) {
		for (int i = 0; i < sizeof(defsGlut) / sizeof(*defsGlut); i += 1) {
			if (defsGlut[i].name == NULL) {
				continue;
			}
			if (!rt->api.ccDefInt(rt->cc, defsGlut[i].name, defsGlut[i].value)) {
				return -3;
			}
		}
		for (int i = 0; i < sizeof(libcGlut) / sizeof(*libcGlut); i += 1) {
			if (!rt->api.ccDefCall(rt->cc, libcGlut[i].fun, libcGlut[i].def)) {
				return -3;
			}
		}
		ccEnd(rt, nsp);
	}
	return 0;
}
