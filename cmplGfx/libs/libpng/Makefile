OUTDIR=..
OBJDIR=obj
SRCDIRZIP=src/zlib-1.2.5/
SRCDIRPNG=src/libpng-1.5.7/

ARH=-m32
CFLAGS=-g0 -Os
CCC=gcc $(ARH)

MKDIR=mkdir -p
ifeq "$(OS)" "Windows_NT"
	MKDIR=mkdir
endif

OBJS=\
	$(OBJDIR)/adler32.o\
	$(OBJDIR)/compress.o\
	$(OBJDIR)/crc32.o\
	$(OBJDIR)/deflate.o\
	$(OBJDIR)/gzclose.o\
	$(OBJDIR)/gzlib.o\
	$(OBJDIR)/gzread.o\
	$(OBJDIR)/gzwrite.o\
	$(OBJDIR)/infback.o\
	$(OBJDIR)/inffast.o\
	$(OBJDIR)/inflate.o\
	$(OBJDIR)/inftrees.o\
	$(OBJDIR)/trees.o\
	$(OBJDIR)/uncompr.o\
	$(OBJDIR)/zutil.o\
\
	$(OBJDIR)/png.o $(OBJDIR)/pngerror.o\
	$(OBJDIR)/pngget.o $(OBJDIR)/pngmem.o $(OBJDIR)/pngpread.o $(OBJDIR)/pngread.o $(OBJDIR)/pngrio.o $(OBJDIR)/pngrtran.o $(OBJDIR)/pngrutil.o\
	$(OBJDIR)/pngset.o $(OBJDIR)/pngtrans.o $(OBJDIR)/pngwio.o $(OBJDIR)/pngwrite.o $(OBJDIR)/pngwtran.o $(OBJDIR)/pngwutil.o\

#~ make library then remove object files
build: lib clean

lib: $(OUTDIR) $(OBJDIR) $(OBJS)
	ar rcs $(OUTDIR)/libpng.a $(OBJS)

clean:
	rm -f $(OBJS)

$(OUTDIR):
	$(MKDIR) "$(OUTDIR)"

$(OBJDIR):
	$(MKDIR) "$(OBJDIR)"

$(OBJDIR)/%.o: $(SRCDIRZIP)%.c
	$(CCC) -c $(CFLAGS) '$<' -o '$@'

$(OBJDIR)/%.o: $(SRCDIRPNG)%.c
	$(CCC) -c $(CFLAGS) -I . -I '$(SRCDIRZIP)' '$<' -o '$@'
