BINDIR=out
GX_OUT=out
CFLAGS=-Wall -g0 -O3
EMFLAGS=-s WASM=1 -s EXPORT_ALL=0 -s INVOKE_RUN=0 -s ALLOW_MEMORY_GROWTH=1 --no-heap-copy
#EMFLAGS+=-s "EXPORTED_FUNCTIONS=['_main','_rtInit','_ccInit','_ccAddUnit','_ccGenCode','_execute']"
EM_EMBED=--preload-file lib/stdlib.ci --preload-file lib/math.ci --preload-file lib/math.Complex.ci --preload-file lib/string.ci

SRC_CC=\
	src/*.h\
	src/*.inl\
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
	libGfx/src/*.h\
	libGfx/src/gx_surf.c\
	libGfx/src/gx_color.c\
	libGfx/src/g2_draw.c\
	libGfx/src/g3_draw.c\
	libGfx/src/g2_image.c\
	libGfx/src/g3_mesh.c\
	libGfx/src/gx_gui.X11.c\
	libGfx/src/gx_main.c

CC_OUT=$(BINDIR)/obj/cc
GX_OUT=$(BINDIR)/obj/gx

cmpl: $(addprefix $(CC_OUT)/, $(notdir $(filter %.o, $(SRC_CC:%.c=%.o))))
	gcc $(CFLAGS) -o $(BINDIR)/cmpl $^ -lm -ldl

libGfx.so: $(addprefix $(GX_OUT)/, $(notdir $(filter %.o, $(SRC_GX:%.c=%.o))))
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.so $^ -lm -ldl -lpng -ljpeg -lX11

libFile.so: lib/src/file.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libFile.so lib/src/file.c

libOpenGL.so: lib/src/openGL.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libOpenGL.so lib/src/openGL.c -lGL -lGLU -lglut

cmpl.js: $(SRC_CC) lib/stdlib.ci
	emcc $(CFLAGS) $(EMFLAGS) $(filter %.c, $^) -o extras/Emscripten/cmpl.js $(EM_EMBED)

cmpl.dbg.js: $(SRC_CC) lib/stdlib.ci
	emcc -g3 -O0 -s WASM=0 $(filter %.c, $^) -o extras/Emscripten/cmpl.dbg.js

clean:
	rm -f -R $(BINDIR)
	mkdir -p $(CC_OUT)
	mkdir -p $(GX_OUT)

$(CC_OUT)/%.o: src/%.c $(filter-out %.c, $(SRC_CC))
	gcc $(CFLAGS) -o $@ -c $<

$(GX_OUT)/%.o: libGfx/src/%.c $(filter-out %.c, $(SRC_CC))
	gcc -fPIC $(CFLAGS) -I src -o $@ -c $<
