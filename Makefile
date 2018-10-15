BINDIR=out
CFLAGS=-Wall -g0 -O3
EMFLAGS=-s WASM=1 -s EXPORT_ALL=0 -s INVOKE_RUN=0 -s ALLOW_MEMORY_GROWTH=1
#EMFLAGS+=-s "EXPORTED_FUNCTIONS=['_main','_rtInit','_ccInit','_ccAddUnit','_ccGenCode','_execute']"

SRC_CC=\
	src/cgen.c\
	src/code.c\
	src/core.c\
	src/lexer.c\
	src/lstd.c\
	src/parser.c\
	src/plugin.c\
	src/printer.c\
	src/tree.c\
	src/type.c\
	src/utils.c\
	src/main.c

SRC_GX=\
	libGfx/src/gx_surf.c\
	libGfx/src/gx_color.c\
	libGfx/src/g2_draw.c\
	libGfx/src/g3_draw.c\
	libGfx/src/g2_image.c\
	libGfx/src/g3_mesh.c\
	libGfx/src/gx_gui.X11.c\
	libGfx/src/gx_main.c

cmpl: $(SRC_CC)
	gcc $(CFLAGS) -o $(BINDIR)/cmpl $(SRC_CC) -lm -ldl

libFile.so: libEtc/src/file.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libFile.so libEtc/src/file.c

libOpenGL.so: libEtc/src/openGL.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libOpenGL.so libEtc/src/openGL.c -lGL -lGLU -lglut

libGfx.so: $(SRC_GX)
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.so $(SRC_GX) -lm -ldl -lpng -ljpeg -lX11

cmpl.js: $(SOURCE) stdlib.ci
	emcc $(CFLAGS) $(EMFLAGS) $(SRC_CC) -o extras/Emscripten/cmpl.js --embed-file stdlib.ci

cmpl.dbg.js: $(SOURCE) stdlib.ci
	emcc -g3 -O0 -s WASM=0 $(SRC_CC) -o extras/Emscripten/cmpl.dbg.js --embed-file stdlib.ci

clean:
	rm -f $(BINDIR)/*
