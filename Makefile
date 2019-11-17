BINDIR?=bin
CC_OUT=$(BINDIR)/obj.cc
GX_OUT=$(BINDIR)/obj.gx
GX_SRC=cmplGfx/src

CFLAGS=-Wall -Wextra -g0 -O3 -std=gnu99
EMFLAGS=-g0 -O3 -s WASM=1 -s EXPORT_ALL=1 -s INVOKE_RUN=0 -s ASSERTIONS=0 -s BINARYEN_TRAP_MODE='clamp' --no-heap-copy
EMFLAGS+=--memory-init-file 0 -s TOTAL_MEMORY=128MB -s WASM_MEM_MAX=1GB -s ALLOW_MEMORY_GROWTH=1

EM_EMBED=$(shell find lib -type f -name '*.ci' -not -path '*/todo/*' -printf '--preload-file %p\n')
EM_SIDE_MODULE=-s SIDE_MODULE=1 -s "EXPORTED_FUNCTIONS=['_cmplInit']"
EM_MAIN_MODULE=-s MAIN_MODULE=1
#EM_MAIN_MODULE+=-s "EXPORTED_FUNCTIONS=['_rtInit','_ccInit','_ccAddUnit','_ccGenCode','_execute']"

ifneq "$(OS)" "Windows_NT"
	CFLAGS+=-D USE_PNG -D USE_JPEG
	EMFLAGS+=-D USE_PNG
	MKDIRF=--parents
	CFLAGS+=-fPIC
else
	CFLAGS+=-D MOC_PNG -D MOC_JPEG
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
	src/utils.c

SRC_GX=\
	$(GX_SRC)/*.h\
	$(GX_SRC)/gx_surf.c\
	$(GX_SRC)/gx_color.c\
	$(GX_SRC)/g2_draw.c\
	$(GX_SRC)/g3_draw.c\
	$(GX_SRC)/g2_image.c\
	$(GX_SRC)/g3_mesh.c\
	$(GX_SRC)/gx_main.c


# for Linux platform
SRC_CC_EXE=$(SRC_CC) src/main.c
cmpl: $(addprefix $(CC_OUT)/, $(notdir $(filter %.o, $(SRC_CC_EXE:%.c=%.o))))
	gcc -o $(BINDIR)/cmpl $^ -lm -ldl

SRC_GX_X11=$(SRC_GX) $(GX_SRC)/os_linux/gx_gui.x11.c $(GX_SRC)/os_linux/time.unx.c
libGfx.so: $(addprefix $(GX_OUT)/, $(notdir $(filter %.o, $(SRC_GX_X11:%.c=%.o))))
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.so $^ -lm -ldl -lpng -ljpeg -lX11

libFile.so: cmplFile/src/file.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libFile.so $^

libOpenGL.so: cmplGL/src/openGL.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libOpenGL.so $^ -lGL -lGLU -lglut


# for Windows platform
cmpl.exe: $(addprefix $(CC_OUT)/, $(notdir $(filter %.o, $(SRC_CC_EXE:%.c=%.o))))
	gcc -o $(BINDIR)/cmpl $^ -lm

SRC_GX_W32=$(SRC_GX) $(GX_SRC)/os_win32/gx_gui.w32.c  $(GX_SRC)/os_win32/time.w32.c
libGfx.dll: $(addprefix $(GX_OUT)/, $(notdir $(filter %.o, $(SRC_GX_W32:%.c=%.o))))
	gcc -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.dll $^ -lm -lgdi32

libFile.dll: cmplFile/src/file.c
	gcc -shared $(CFLAGS) -I src -o $(BINDIR)/libFile.dll $^

libOpenGL.dll: cmplGL/src/openGL.c
	gcc -shared $(CFLAGS) -I src -o $(BINDIR)/libOpenGL.dll $^ -lopengl32 -lglu32 -lglut32


# for Browser platform
cmpl.js: $(SRC_CC_EXE) lib/stdlib.ci
	emcc $(EMFLAGS) -o extras/demo/emscripten/cmpl.js $(EM_MAIN_MODULE) -s USE_SDL=2 -s USE_LIBPNG=1 $(filter %.c, $^) $(EM_EMBED)

libFile.wasm: cmplFile/src/file.c
	emcc $(EMFLAGS) -o extras/demo/emscripten/libFile.wasm -I src $(EM_SIDE_MODULE) $(filter %.c, $^)

libGfx.wasm: $(SRC_GX) $(GX_SRC)/os_linux/gx_gui.sdl.c  $(GX_SRC)/os_linux/time.unx.c
	emcc $(EMFLAGS) -o extras/demo/emscripten/libGfx.wasm -I src $(EM_SIDE_MODULE) -s USE_SDL=2 -s USE_LIBPNG=1 $(filter %.c, $^)

cmpl.dbg.js: $(SRC_CC_EXE) lib/stdlib.ci
	emcc -g3 -O0 -s WASM=0 $(filter %.c, $^) -o extras/demo/emscripten/cmpl.dbg.js


clean:
	rm -f -R $(BINDIR) $(CC_OUT) $(GX_OUT)
	mkdir $(MKDIRF) "$(BINDIR)"
	mkdir $(MKDIRF) "$(CC_OUT)"
	mkdir $(MKDIRF) "$(GX_OUT)"

$(CC_OUT)/%.o: src/%.c $(filter-out %.c, $(SRC_CC_EXE))
	gcc $(CFLAGS) -o "$@" -c "$<"

$(GX_OUT)/%.o: $(GX_SRC)/%.c $(filter-out %.c, $(SRC_CC_EXE))
	gcc $(CFLAGS) -I src -o "$@" -c "$<"

$(GX_OUT)/%.o: $(GX_SRC)/os_win32/%.c $(filter-out %.c, $(SRC_CC_EXE))
	gcc $(CFLAGS) -I src -o "$@" -c "$<"

$(GX_OUT)/%.o: $(GX_SRC)/os_linux/%.c $(filter-out %.c, $(SRC_CC_EXE))
	gcc $(CFLAGS) -I src -o "$@" -c "$<"
