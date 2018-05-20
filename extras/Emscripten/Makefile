
BASEDIR=../..

SRCDIR=$(BASEDIR)/src

SOURCE=\
	$(SRCDIR)/cgen.c\
	$(SRCDIR)/code.c\
	$(SRCDIR)/core.c\
	$(SRCDIR)/lexer.c\
	$(SRCDIR)/lstd.c\
	$(SRCDIR)/parser.c\
	$(SRCDIR)/plugin.c\
	$(SRCDIR)/printer.c\
	$(SRCDIR)/tree.c\
	$(SRCDIR)/type.c\
	$(SRCDIR)/utils.c\
	$(SRCDIR)/main.c

CFLAGS=-O3
OPTIONS=-s WASM=1
OPTIONS+=-s EXPORT_ALL=0
OPTIONS+=-s INVOKE_RUN=0
OPTIONS+=-s ALLOW_MEMORY_GROWTH=1
#OPTIONS+=-s "EXPORTED_FUNCTIONS=['_main','_rtInit','_ccInit','_ccAddUnit','_ccGenCode','_execute']"

cmpl.js: $(SOURCE) workspace
	emcc $(CFLAGS) $(OPTIONS) $(SOURCE) -o cmpl.js --preload-file workspace
	rm -R workspace

workspace:
	mkdir workspace
	cp $(BASEDIR)/stdlib.ci ./workspace/stdlib.ci
	cp $(BASEDIR)/test/lang.function.ci ./workspace/function.ci
	cp $(BASEDIR)/test/stdc.nfcTryExec.ci ./workspace/execute.ci
