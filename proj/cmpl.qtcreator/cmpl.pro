CONFIG += console
CONFIG -= qt

CC_DIR = ../../src

HEADERS += \
	$$CC_DIR/cmpl.h \
	$$CC_DIR/cmplVm.h \
	$$CC_DIR/cmplCc.h \
	$$CC_DIR/parser.h \
	$$CC_DIR/printer.h \
	$$CC_DIR/internal.h

SOURCES += \
	$$CC_DIR/cgen.c \
	$$CC_DIR/code.c \
	$$CC_DIR/core.c \
	$$CC_DIR/lexer.c \
	$$CC_DIR/lstd.c \
	$$CC_DIR/parser.c \
	$$CC_DIR/plugin.c \
	$$CC_DIR/printer.c \
	$$CC_DIR/tree.c \
	$$CC_DIR/type.c \
	$$CC_DIR/utils.c \
	$$CC_DIR/main.c

OTHER_FILES += \
	$$CC_DIR/defs.inl \
	$$CC_DIR/code.inl \
	$$CC_DIR/../*.ci
	$$CC_DIR/../test/*.ci

isEmpty(ARCH) {
	ARCH = 64
}

win32 {
	LIBS += -lm
}

unix {
	LIBS += -lm -ldl
	QMAKE_LINK = gcc
	QMAKE_CFLAGS += -m$$ARCH
	QMAKE_LFLAGS += -m$$ARCH
}
