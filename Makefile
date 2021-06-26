BINDIR?=bin
CC_OUT=$(BINDIR)/obj
GX_OUT=$(BINDIR)/obj
GX_SRC=cmplGfx/src

CFLAGS=-Wall -Wextra -g0 -O3 -std=gnu99
EMFLAGS=-g0 -O3 -s WASM=1 -s EXPORT_ALL=1 -s INVOKE_RUN=0 -s ASSERTIONS=0 -s BINARYEN_TRAP_MODE='clamp' --no-heap-copy
EMFLAGS+=--memory-init-file 0 -s TOTAL_MEMORY=128MB -s WASM_MEM_MAX=1GB -s ALLOW_MEMORY_GROWTH=1
EMFLAGS+=-D NO_LIBJPEG -D NO_LIBPNG

EM_EMBED='--preload-file' 'cmplStd/stdlib.ci' '--preload-file' 'cmplGfx/gfxlib.ci'
EM_EMBED+=$(shell find cmplStd/lib -type f -name '*.ci' -not -path '*/todo/*' -printf '--preload-file %p\n')
EM_EMBED+=$(shell find cmplGfx/lib -type f -name '*.ci' -not -path '*/todo/*' -printf '--preload-file %p\n')
EM_SIDE_MODULE=-s SIDE_MODULE=1 -s "EXPORTED_FUNCTIONS=['_cmplInit']"
EM_MAIN_MODULE=-s MAIN_MODULE=1 -lidbfs.js

CLIBS=-lm
GXLIBS=-lm
CFLAGS+=-I libs/stb
EMFLAGS+=-I libs/stb

ifneq "$(OS)" "Windows_NT"
	GXLIBS+=-lpng -ljpeg
	CFLAGS+=-fPIC
	CLIBS+=-ldl
	UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
		MKDIRF=--parents
    endif
    ifeq ($(UNAME_S),Darwin)
		MKDIRF=-p
    endif
else
	#CFLAGS+=-D NO_LIBPNG -D NO_LIBJPEG
	CFLAGS+=-I libs/libjpeg
	CFLAGS+=-I libs/libpng
	GXLIBS+=-lpng -ljpeg
	CFLAGS+=-L libs
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
	$(GX_SRC)/gx_gradient.c\
	$(GX_SRC)/g2_draw.c\
	$(GX_SRC)/g3_draw.c\
	$(GX_SRC)/g2_image.c\
	$(GX_SRC)/g3_mesh.c\
	$(GX_SRC)/gx_cmpl.c


SRC_GX_X11=$(SRC_GX) $(GX_SRC)/os_linux/gx_gui.x11.c $(GX_SRC)/os_linux/time.unx.c
SRC_GX_SDL=$(SRC_GX) $(GX_SRC)/os_linux/gx_gui.sdl.c $(GX_SRC)/os_linux/time.unx.c
SRC_GX_W32=$(SRC_GX) $(GX_SRC)/os_win32/gx_gui.w32.c $(GX_SRC)/os_win32/time.w32.c
SRC_CC_EXE=$(SRC_CC) src/main.c
cmpl: $(addprefix $(CC_OUT)/, $(notdir $(filter %.o, $(SRC_CC_EXE:%.c=%.o))))
	gcc -o $(BINDIR)/cmpl $^ $(CLIBS)

# Linux platform
libGfx11.so: $(addprefix $(GX_OUT)/, $(notdir $(filter %.o, $(SRC_GX_X11:%.c=%.o))))
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.so $^ $(GXLIBS) -lX11

libGfx.so: $(addprefix $(GX_OUT)/, $(notdir $(filter %.o, $(SRC_GX_SDL:%.c=%.o))))
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.so $^ $(GXLIBS) -lSDL2

libFile.so: cmplFile/src/file.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libFile.so $^

libOpenGL.so: cmplGL/src/openGL.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libOpenGL.so $^ -lGL -lGLU -lglut


# Macos platform
libGfx11.dylib: $(addprefix $(GX_OUT)/, $(notdir $(filter %.o, $(SRC_GX_X11:%.c=%.o))))
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.dylib $^ $(GXLIBS)g -lX11

libGfx.dylib: $(addprefix $(GX_OUT)/, $(notdir $(filter %.o, $(SRC_GX_SDL:%.c=%.o))))
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.dylib $^ $(GXLIBS) -lSDL2

libFile.dylib: cmplFile/src/file.c
	gcc -fPIC -shared $(CFLAGS) -I src -o $(BINDIR)/libFile.dylib $^


# Windows platform
libGfx.dll: $(addprefix $(GX_OUT)/, $(notdir $(filter %.o, $(SRC_GX_W32:%.c=%.o))))
	gcc -shared $(CFLAGS) -I src -o $(BINDIR)/libGfx.dll $^ $(GXLIBS) -lgdi32

libFile.dll: cmplFile/src/file.c
	gcc -shared $(CFLAGS) -I src -o $(BINDIR)/libFile.dll $^

libOpenGL.dll: cmplGL/src/openGL.c
	gcc -shared $(CFLAGS) -I src -o $(BINDIR)/libOpenGL.dll $^ -lopengl32 -lglu32 -lglut32


# Webassembly platform
cmpl.js: $(SRC_CC_EXE) cmplStd/stdlib.ci
	emcc $(EMFLAGS) -o $(BINDIR)/cmpl.js $(EM_MAIN_MODULE) -s USE_SDL=2 $(filter %.c, $^) $(EM_EMBED)

libFile.wasm: cmplFile/src/file.c
	emcc $(EMFLAGS) -o $(BINDIR)/libFile.wasm -I src $(EM_SIDE_MODULE) $(filter %.c, $^)

libGfx.wasm: $(SRC_GX) $(GX_SRC)/os_linux/gx_gui.sdl.c $(GX_SRC)/os_linux/time.unx.c
	emcc $(EMFLAGS) -o $(BINDIR)/libGfx.wasm -I src $(EM_SIDE_MODULE) -s USE_SDL=2 $(filter %.c, $^)

cmpl.dbg.js: $(SRC_CC_EXE) cmplStd/stdlib.ci
	emcc -g3 -O0 -s WASM=0 $(filter %.c, $^) -o $(BINDIR)/cmpl.dbg.js


clean:
	rm -f -R $(BINDIR) $(CC_OUT) $(GX_OUT)
	mkdir $(MKDIRF) "$(BINDIR)" "$(CC_OUT)" "$(GX_OUT)"

$(CC_OUT)/%.o: src/%.c $(filter-out %.c, $(SRC_CC_EXE))
	gcc $(CFLAGS) -o "$@" -c "$<"

$(GX_OUT)/%.o: $(GX_SRC)/%.c $(filter-out %.c, $(SRC_CC_EXE))
	gcc $(CFLAGS) -I src -o "$@" -c "$<"

$(GX_OUT)/%.o: $(GX_SRC)/os_win32/%.c $(filter-out %.c, $(SRC_CC_EXE))
	gcc $(CFLAGS) -I src -o "$@" -c "$<"

$(GX_OUT)/%.o: $(GX_SRC)/os_linux/%.c $(filter-out %.c, $(SRC_CC_EXE))
	gcc $(CFLAGS) -I src -o "$@" -c "$<"
