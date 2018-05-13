
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

cmpl.js: $(SOURCE) stdlib.ci function.ci execute.ci
	emcc $(CFLAGS) $(OPTIONS) $(SOURCE) -o cmpl.js --preload-file stdlib.ci --preload-file function.ci --preload-file execute.ci
	rm stdlib.ci function.ci execute.ci

stdlib.ci: $(BASEDIR)/stdlib.ci
	cp $(BASEDIR)/stdlib.ci .

function.ci: $(BASEDIR)/test/lang.function.ci
	cp $(BASEDIR)/test/lang.function.ci ./function.ci

execute.ci: $(BASEDIR)/test/stdc.nfcTryExec.ci
	cp $(BASEDIR)/test/stdc.nfcTryExec.ci ./execute.ci
