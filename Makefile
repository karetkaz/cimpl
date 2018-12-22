BINDIR?=bin
CC_OUT=$(BINDIR)/obj.cc
GX_OUT=$(BINDIR)/obj.gx
GX_SRC=lib/cmplGfx/src

CFLAGS=-Wall -g0 -O3 -std=gnu99
EMFLAGS=-s WASM=1 -s EXPORT_ALL=0 -s INVOKE_RUN=0 -s ALLOW_MEMORY_GROWTH=1 --no-heap-copy
#EMFLAGS+=-s "EXPORTED_FUNCTIONS=['_main','_rtInit','_ccInit','_ccAddUnit','_ccGenCode','_execute']"
EM_EMBED=--preload-file lib/stdlib.ci --preload-file lib/std/math.ci --preload-file lib/std/math.Complex.ci --preload-file lib/std/string.ci

ifneq "$(OS)" "Windows_NT"
	MKDIRF=--parents
	CFLAGS+=-fPIC
endif

SRC_CC=\
	src/*.h\
	src/*.inl\
	src/cgen.c\
	src/code.c\
	src/core.c\
	src/lexer.c\
	src/lstd.c\
	src/parser.c\
	src/platform.c\
	src/printer.c\
	src/tree.c\
	src/type.c\
	src/utils.c\
	src/main.c

SRC_GX=\
	$GX_SRC/*.h\
	$GX_SRC/gx_surf.c\
	$GX_SRC/gx_color.c\
	$GX_SRC/g2_draw.c\
	$GX_SRC/g3_draw.c\
	$GX_SRC/g2_image.c\
	$GX_SRC/g3_mesh.c\
	$GX_SRC/gx_main.c


# for Linux platform
cmpl: $(addprefix $(CC_OUT)/, $(notdir $(filter %.o, $(SRC_CC:%.c=%.o))))
	gcc -o $(BINDIR)/cmpl $^ -lm -ldl

SRC_GX_X11=$(SRC_GX) $GX_SRC/gx_gui.X11.c
libGfx.so: $(addprefix $(GX_OUT)/, $(notdir $(filter %.o, $(SRC_GX_X11:%.c=%.o))))
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.so $^ -lm -ldl -lpng -ljpeg -lX11

libFile.so: lib/cmplFile/src/file.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libFile.so $^

libOpenGL.so: lib/cmplGL/src/openGL.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libOpenGL.so $^ -lGL -lGLU -lglut


# for Windows platform
cmpl.exe: $(addprefix $(CC_OUT)/, $(notdir $(filter %.o, $(SRC_CC:%.c=%.o))))
	gcc -o $(BINDIR)/cmpl $^ -lm

SRC_GX_W32=$(SRC_GX) $GX_SRC/gx_gui.W32.c
libGfx.dll: $(addprefix $(GX_OUT)/, $(notdir $(filter %.o, $(SRC_GX_W32:%.c=%.o))))
	gcc -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.dll $^ -lm -lgdi32

libFile.dll: lib/cmplFile/src/file.c
	gcc -shared $(CFLAGS) -I src -o $(BINDIR)/libFile.dll $^

libOpenGL.dll: lib/cmplGL/src/openGL.c
	gcc -shared $(CFLAGS) -I src -o $(BINDIR)/libOpenGL.dll $^ -lopengl32 -lglu32 -lglut32


# for Browser platform
cmpl.js: $(SRC_CC) lib/stdlib.ci
	emcc -g0 -O3 $(EMFLAGS) $(filter %.c, $^) -o extras/Emscripten/cmpl.js $(EM_EMBED)

cmpl.dbg.js: $(SRC_CC) lib/stdlib.ci
	emcc -g3 -O0 -s WASM=0 $(filter %.c, $^) -o extras/Emscripten/cmpl.dbg.js


clean:
	rm -f -R $(BINDIR) $(CC_OUT) $(GX_OUT)
	mkdir $(MKDIRF) "$(BINDIR)"
	mkdir $(MKDIRF) "$(CC_OUT)"
	mkdir $(MKDIRF) "$(GX_OUT)"

$(CC_OUT)/%.o: src/%.c $(filter-out %.c, $(SRC_CC))
	gcc $(CFLAGS) -o $@ -c $<

$(GX_OUT)/%.o: $(GX_SRC)/%.c $(filter-out %.c, $(SRC_CC))
	gcc $(CFLAGS) -I src -o $@ -c $<
