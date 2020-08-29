#~ REM ~ View SRCDIRJPEG/filelist.txt
#~ REM ~ jpeg.h: #include "SRCDIRJPEG/jpeglib.h"
#~ REM ~ rgb->bgr: SRCDIRJPEG/jmorecfg.h

OUTDIR=..
OBJDIR=obj
SRCDIRJPEG=src/jpeg-8d/

ARH=-m32
CFLAGS=-g0 -Os
CCC=gcc $(ARH)

MKDIR=mkdir -p
ifeq "$(OS)" "Windows_NT"
	MKDIR=mkdir
endif

OBJS=\
	$(OBJDIR)/jerror.o\
	$(OBJDIR)/jutils.o\
	$(OBJDIR)/jmemmgr.o\
	$(OBJDIR)/jmemansi.o\
\
	$(OBJDIR)/jdmaster.o\
	$(OBJDIR)/jdinput.o\
	$(OBJDIR)/jdmainct.o\
	$(OBJDIR)/jdcoefct.o\
	$(OBJDIR)/jdpostct.o\
	$(OBJDIR)/jdmarker.o\
	$(OBJDIR)/jdhuff.o\
	$(OBJDIR)/jdarith.o\
	$(OBJDIR)/jddctmgr.o\
	$(OBJDIR)/jidctint.o\
	$(OBJDIR)/jidctfst.o\
	$(OBJDIR)/jidctflt.o\
	$(OBJDIR)/jdsample.o\
	$(OBJDIR)/jdcolor.o\
	$(OBJDIR)/jdmerge.o\
	$(OBJDIR)/jquant1.o\
	$(OBJDIR)/jquant2.o\
	$(OBJDIR)/jdatasrc.o\
\
	$(OBJDIR)/jcomapi.o\
	$(OBJDIR)/jaricom.o\
	$(OBJDIR)/jdapimin.o\
	$(OBJDIR)/jdapistd.o

#~ make library then remove object files
build: lib clean

lib: $(OUTDIR) $(OBJDIR) $(OBJS)
	ar rcs $(OUTDIR)/libjpeg.a $(OBJS)

clean:
	rm -f $(OBJS)

$(OUTDIR):
	$(MKDIR) "$(OUTDIR)"

$(OBJDIR):
	$(MKDIR) "$(OBJDIR)"

$(OBJDIR)/%.o: $(SRCDIRJPEG)%.c
	$(CCC) -c $(CFLAGS) -I . '$<' -o '$@'
