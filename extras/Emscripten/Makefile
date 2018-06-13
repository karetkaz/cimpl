
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

CFLAGS=-Wall -g0 -O3
OPTIONS=-s WASM=1
OPTIONS+=-s EXPORT_ALL=0
OPTIONS+=-s INVOKE_RUN=0
#OPTIONS+=-s ASSERTIONS=1
OPTIONS+=-s ALLOW_MEMORY_GROWTH=1
#OPTIONS+=-s "EXPORTED_FUNCTIONS=['_main','_rtInit','_ccInit','_ccAddUnit','_ccGenCode','_execute']"

cmpl.js: $(SOURCE) stdlib.ci workspace
	emcc $(CFLAGS) $(OPTIONS) $(SOURCE) -o cmpl.js --preload-file stdlib.ci --preload-file workspace
	rm -R workspace
	rm stdlib.ci

stdlib.ci:
	cp $(BASEDIR)/stdlib.ci ./stdlib.ci

workspace:
	mkdir workspace
	#cp $(BASEDIR)/test/*.ci ./workspace/
	cp $(BASEDIR)/test/lang.inlineEmit.ci ./workspace/emit.ci
	cp $(BASEDIR)/test/lang.reflection.ci ./workspace/reflect.ci
	cp $(BASEDIR)/test/stdc.nfcMemory.ci ./workspace/memory.ci
	cp $(BASEDIR)/test/stdc.nfcNumber.ci ./workspace/number.ci
	cp $(BASEDIR)/test/lang.function.ci ./workspace/function.ci
	cp $(BASEDIR)/test/stdc.nfcTryExec.ci ./workspace/execute.ci
