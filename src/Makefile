CFLAGS=-Wall -g0 -O3
#~ CFLAGS=-Wall -g3 -ggdb
#~ CFLAGS=-pipe -g -Wall -Wextra -Wpadded
ARH=-m32
ARH=-m64
LIBS=-lm -ldl
MKDIR=mkdir -p

ifeq "$(OS)" "Windows_NT"
	LIBS=-lm
	MKDIR=mkdir
endif

ifndef SRCDIR
ifeq "$(OS)" "Windows_NT"
	SRCDIR=.
else
	SRCDIR=$(shell pwd)
endif
endif

ifndef OBJDIR
	OBJDIR=../out/obj
endif
ifndef BINDIR
	BINDIR=../out
endif

OBJS=\
	$(OBJDIR)/tree.o\
	$(OBJDIR)/type.o\
	$(OBJDIR)/lexer.o\
	$(OBJDIR)/parser.o\
	$(OBJDIR)/printer.o\
	$(OBJDIR)/lstd.o\
	$(OBJDIR)/code.o\
	$(OBJDIR)/cgen.o\
	$(OBJDIR)/core.o\
	$(OBJDIR)/utils.o\
	$(OBJDIR)/main.o\
	$(OBJDIR)/plugin.o

build:$(OBJDIR) $(OBJS)
	cc $(ARH) -o $(BINDIR)/cmpl $(OBJS) $(LIBS)

$(OBJDIR):
	$(MKDIR) "$(OBJDIR)"

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/*.h $(SRCDIR)/*.inl
	cc $(ARH) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS) $(OBJDIR)/main.o
