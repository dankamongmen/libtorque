.DELETE_ON_ERROR:
.PHONY: all test testarchdetect testtorquehost hardtest testssl doc clean \
	mrproper flow install unsafe-install deinstall
.DEFAULT_GOAL:=test

# Shared object versioning. MAJORVER will become 1 upon the first stable
# release, and at that point changes only when the API changes. The minor
# and release versions change more arbitrarily. Only MAJORVER is in the soname.
MAJORVER:=0
MINORVER:=0
RELEASEVER:=1

# Functions. Invoke with $(call funcname,comma,delimited,args).
which = $(firstword $(wildcard $(addsuffix /$(1),$(subst :, ,$(PATH)))))

# Don't run shell commands unnecessarily. Cache commonly-used results here.
UNAME:=$(shell uname)

######################################################################
# USER SPECIFICATION AREA BEGINS
#
# Variables defined with ?= can be inherited from the environment, and thus
# specified. Provide the defaults here. Document these in the README.
-include GNUmakefile.local

PREFIX?=/usr/local
ifeq ($(UNAME),FreeBSD)
DOCPREFIX?=$(PREFIX)/man
else
DOCPREFIX?=$(PREFIX)/share/man
endif

# Some systems don't install exuberant-ctags as 'ctags'. Some people use etags.
TAGBIN?=$(firstword $(call which,exctags) $(call which,ctags))

# We want GCC 4.3+ if we can find it. Some systems have install it as gcc-v.v,
# some as gccv.v, some will have a suitably up-to-date default gcc...bleh.
ifeq "$(origin CC)" "default"
CC:=$(shell (which gcc-4.4 || which gcc44 || which gcc-4.3 || which gcc43 || echo gcc) 2>/dev/null)
endif

# We compile for the host µ-architecture/ISA, providing the "native" option to
# gcc's -march and -mtune. If you don't have gcc 4.3 or greater, you'll need to
# define appropriate march and mtune values for your system (see gcc's
# "Submodel Options" info page). Libraries intended to be run on arbitrary x86
# hardware must be built with MARCH defined as "generic", and MTUNE unset.
MARCH?=native
ifneq ($(MARCH),generic)
MTUNE?=native
endif

ifndef LIBTORQUE_WITHOUT_ADNS
LIBFLAGS+=-ladns
else
DFLAGS+=-DLIBTORQUE_WITHOUT_ADNS
endif

ifndef LIBTORQUE_WITHOUT_SSL
IFLAGS+=$(shell pkg-config --cflags openssl)
LIBFLAGS+=$(shell (pkg-config --libs openssl || echo -lssl -lcrypto))
else
DFLAGS+=-DLIBTORQUE_WITHOUT_SSL
endif

ifeq ($(UNAME),FreeBSD)
DFLAGS+=-DLIBTORQUE_WITHOUT_NUMA
else
ifndef LIBTORQUE_WITHOUT_NUMA
LIBFLAGS+=-lnuma
else
DFLAGS+=-DLIBTORQUE_WITHOUT_NUMA
endif
endif

CUDADIR?=/usr/

ifeq ($(UNAME),FreeBSD)
DFLAGS+=-DLIBTORQUE_WITHOUT_CUDA
else
ifndef LIBTORQUE_WITHOUT_CUDA
LIBFLAGS+=-L$(CUDADIR)/lib -lcuda
IFLAGS+=-I$(CUDADIR)/include
else
DFLAGS+=-DLIBTORQUE_WITHOUT_CUDA
endif
endif

ifndef LIBTORQUE_WITHOUT_WERROR
WFLAGS+=-Werror
endif

# Any old XSLT processor ought do, but you might need change the invocation.
XSLTPROC?=$(shell (which xsltproc || echo xsltproc) 2> /dev/null)
# This can be a URL; it's the docbook-to-manpage XSL
#DOC2MANXSL?=--nonet /usr/share/xml/docbook/stylesheet/docbook-xsl/manpages/docbook.xsl

DOT?=$(shell (which dot) 2> /dev/null || echo dot)
BIBTEX?=$(shell (which bibtex) 2> /dev/null || echo bibtex)
LATEX?=$(shell (which pdflatex) 2> /dev/null || echo pdflatex) -halt-on-error

#
# USER SPECIFICATION AREA ENDS
######################################################################

# Unilateral definitions, shielded from the environment (save as components).

# System-specific variables closed to external specification
ifeq ($(UNAME),Linux)
DFLAGS+=-DTORQUE_LINUX -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
LFLAGS+=-Wl,--warn-shared-textrel
MANBIN:=mandb
LDCONFIG:=ldconfig
else
ifeq ($(UNAME),FreeBSD)
DFLAGS+=-DTORQUE_FREEBSD
MT_DFLAGS:=-D_THREAD_SAFE -D_POSIX_PTHREAD_SEMANTICS
MANBIN:=makewhatis
LDCONFIG:=ldconfig -m
LFLAGS+=-L/usr/local/lib
else
ifeq ($(UNAME),SunOS)
DFLAGS+=-DTORQUE_SOLARIS
# FIXME
endif
endif
endif

INSTALL:=install -v

# Codenames are factored out, to accommodate changing them later.
ARCHDETECT:=archdetect
SSLSRV:=torquessl
TORQUE:=torque
TORQUEHOST:=torquehost

# Avoid unnecessary uses of 'pwd'; absolute paths aren't as robust as relative
# paths against overlong total path names.
OUT:=.out
SRCDIR:=src
TOOLDIR:=tools
MANDIR:=doc/man
FIGDIR:=doc/figures
PDFDIR:=doc/paper
CSRCDIRS:=$(wildcard $(SRCDIR)/*)
ARCHDETECTDIRS:=$(SRCDIR)/$(ARCHDETECT)
TORQUEDIRS:=$(SRCDIR)/lib$(TORQUE)
TORQUEHOSTDIRS:=$(SRCDIR)/$(TORQUEHOST)

# Simple compositions from here on out
LIBOUT:=$(OUT)/lib
BINOUT:=$(OUT)/bin
TESTOUT:=$(OUT)/test
TORQUESOL:=lib$(TORQUE).so
TORQUESOR:=$(TORQUESOL).$(MAJORVER)
TORQUEREAL:=$(TORQUESOR).$(MINORVER).$(RELEASEVER)
TORQUESTAT:=lib$(TORQUE).a
PKGCONFIG:=$(TOOLDIR)/lib$(TORQUE).pc

# We don't want to have to list all our source files, so discover them based on
# the per-language directory specifications above.
CSRC:=$(shell find $(CSRCDIRS) -type f -name \*.c -print)
CINC:=$(shell find $(CSRCDIRS) -type f -name \*.h -print)
ARCHDETECTSRC:=$(foreach dir, $(ARCHDETECTDIRS), $(filter $(dir)/%, $(CSRC)))
ARCHDETECTOBJ:=$(addprefix $(OUT)/,$(ARCHDETECTSRC:%.c=%.o))
TORQUESRC:=$(foreach dir, $(TORQUEDIRS), $(filter $(dir)/%, $(CSRC)))
TORQUEOBJ:=$(addprefix $(OUT)/,$(TORQUESRC:%.c=%.o))
TORQUEHOSTSRC:=$(foreach dir, $(TORQUEHOSTDIRS), $(filter $(dir)/%, $(CSRC)))
TORQUEHOSTOBJ:=$(addprefix $(OUT)/,$(TORQUEHOSTSRC:%.c=%.o))
SRC:=$(CSRC)
TESTBINS:=$(addprefix $(BINOUT)/,$(notdir $(basename $(wildcard $(TOOLDIR)/testing/*))))
ifndef LIBTORQUE_WITHOUT_EV
TESTBINS+=$(addprefix $(BINOUT)/libev-,signalrx)
endif
BINS:=$(addprefix $(BINOUT)/,$(ARCHDETECT))
ifndef LIBTORQUE_WITHOUT_ADNS
BINS+=$(addprefix $(BINOUT)/,$(TORQUEHOST))
endif
LIBS:=$(addprefix $(LIBOUT)/,$(TORQUESOL) $(TORQUESOR) $(TORQUESTAT))
REALSOS:=$(addprefix $(LIBOUT)/,$(TORQUEREAL))

# Documentation processing
MAN1SRC:=$(wildcard $(MANDIR)/man1/*)
MAN3SRC:=$(wildcard $(MANDIR)/man3/*)
FIGSRC:=$(wildcard $(FIGDIR)/*.dot)
PDFSRC:=$(wildcard $(PDFDIR)/*.tex)
MAN1OBJ:=$(addprefix $(OUT)/,$(MAN1SRC:%.xml=%.1torque))
MAN3OBJ:=$(addprefix $(OUT)/,$(MAN3SRC:%.xml=%.3torque))
FIGDOC:=$(addprefix $(OUT)/,$(FIGSRC:%.dot=%.svg))
PAPER:=$(addprefix $(OUT)/,$(PDFSRC:%.tex=%.pdf))
DOCS:=$(MAN1OBJ) $(MAN3OBJ) $(FIGDOC) $(PAPER)
PRETTYDOT:=$(FIGDIR)/notugly/notugly.xsl
INCINSTALL:=$(addprefix $(SRCDIR)/lib$(TORQUE)/,$(TORQUE).h)
TAGS:=.tags

# Anything that all source->object translations ought dep on. We currently
# include all header files in this list; it'd be nice to refine that FIXME.
# We don't include TAGS due to too much rebuilding as a result:
# http://dank.qemfd.net/bugzilla/show_bug.cgi?id=92
GLOBOBJDEPS:=$(CINC) $(MAKEFILE_LIST)

# Debugging flags. Normally unused, but uncomment the 2nd line to enable.
DEBUGFLAGS:=-rdynamic -g -D_FORTIFY_SOURCE=2
DFLAGS+=$(DEBUGFLAGS)

# Main compilation flags. Define with += to inherit from system-specific flags.
IFLAGS+=-I$(SRCDIR)
MFLAGS:=-fpic -march=$(MARCH)
ifdef MTUNE
MFLAGS+=-mtune=$(MTUNE)
endif
# Not using: -Wpadded, -Wconversion, -Wstrict-overflow=(>1)
WFLAGS+=-Wall -W -Wextra -Wmissing-prototypes -Wundef -Wshadow \
        -Wstrict-prototypes -Wmissing-declarations -Wnested-externs \
        -Wsign-compare -Wpointer-arith -Wbad-function-cast -Wcast-qual \
        -Wdeclaration-after-statement -Wfloat-equal -Wpacked -Winvalid-pch \
        -Wdisabled-optimization -Wcast-align -Wformat -Wformat-security \
        -Wold-style-definition -Woverlength-strings -Wwrite-strings \
	-Wstrict-aliasing=3 -Wunsafe-loop-optimizations -Wstrict-overflow=1
# We get the following from -O (taken from gcc 4.3 docs)
# -fauto-inc-dec -fcprop-registers -fdce -fdefer-pop -fdelayed-branch -fdse \
# -fguess-branch-probability -fif-conversion2 -fif-conversion \
# -finline-small-functions -fipa-pure-const -fipa-reference -fmerge-constants \
# -fsplit-wide-types -ftree-ccp -ftree-ch -ftree-copyrename -ftree-dce \
# -ftree-dominator-opts -ftree-dse -ftree-fre -ftree-sra -ftree-ter -ftree-sink \
# -funit-at-a-time, "and -fomit-frame-pointer on machines where it doesn't
# interfere with debugging."

# We add the following with -O2 (taken from gcc 4.3 docs)
# -fthread-jumps -falign-functions  -falign-jumps -falign-loops -falign-labels \
# -fcaller-saves -fcrossjumping -fcse-follow-jumps  -fcse-skip-blocks \
# -fdelete-null-pointer-checks -fexpensive-optimizations -fgcse -fgcse-lm \
# -foptimize-sibling-calls -fpeephole2 -fregmove -freorder-blocks \
# -freorder-functions -frerun-cse-after-loop -fsched-interblock -fsched-spec \
# -fschedule-insns -fschedule-insns2 -fstrict-aliasing -fstrict-overflow \
# -ftree-pre -ftree-store-ccp -ftree-vrp

# -O3 gets the following:
# -finline-functions -funswitch-loops -fpredictive-commoning -ftree-vectorize \
# -fgcse-after-reload

# The following aren't bound to any -O level:
# -fipa-pta -fipa-cp -ftree-loop-linear -ftree-loop-im -ftree-loop-ivcanon \
# -funsafe-loop-optimizations
# The following also require profiling info:
# -fipa-matrix-reorg
# These require -pthread:
# -ftree-parallelize-loops
OFLAGS+=-O2 -fomit-frame-pointer -finline-functions -fdiagnostics-show-option \
	-fvisibility=hidden -fipa-cp -ftree-loop-linear -ftree-loop-im \
	-ftree-loop-ivcanon -fno-common -ftree-vectorizer-verbose=5
#OFLAGS+=-fdump-tree-all
CFLAGS+=-pipe -std=gnu99 $(DFLAGS)
MT_CFLAGS:=$(CFLAGS) -pthread $(MT_DFLAGS)
CFLAGS+=$(IFLAGS) $(MFLAGS) $(OFLAGS) $(WFLAGS)
MT_CFLAGS+=$(IFLAGS) $(MFLAGS) $(OFLAGS) $(WFLAGS)
LIBFLAGS+=-lpthread
LFLAGS+=-Wl,-O,--default-symver,--enable-new-dtags,--as-needed,--warn-common \
	-Wl,--fatal-warnings,-z,noexecstack,-z,combreloc
ARCHDETECTCFLAGS:=$(CFLAGS)
ARCHDETECTLFLAGS:=$(LFLAGS) -L$(LIBOUT) -ltorque
TORQUECFLAGS:=$(MT_CFLAGS) -shared
TORQUELFLAGS:=$(LFLAGS) -Wl,-soname,$(TORQUESOR) $(LIBFLAGS)
TORQUEHOSTCFLAGS:=$(CFLAGS)
TORQUEHOSTLFLAGS:=$(LFLAGS) -L$(LIBOUT) -ltorque
TESTBINCFLAGS:=$(MT_CFLAGS)
TESTBINLFLAGS:=$(LFLAGS) -Wl,-R$(LIBOUT) -L$(LIBOUT) -ltorque
EVTESTBINLFLAGS:=$(LFLAGS) -lev

flow:
	cflow $(IFLAGS) $(DFLAGS) $(TORQUESRC) 2>/dev/null

# In addition to the binaries and unit tests, 'all' builds documentation,
# packaging, graphs, and all that kind of crap.
all: test doc

doc: $(TAGS) $(DOCS)

test: $(TAGS) $(BINS) $(LIBS) $(TESTBINS) testarchdetect testtorquehost

testarchdetect: $(BINOUT)/$(ARCHDETECT)
	@echo -n "Testing $(<F): "
	env LD_LIBRARY_PATH=$(LIBOUT) $<

ifndef LIBTORQUE_WITHOUT_ADNS
testtorquehost: $(BINOUT)/$(TORQUEHOST)
	@echo -n "Testing $(<F): "
	env LD_LIBRARY_PATH=$(LIBOUT) $< localhost
else
testtorquehost:
endif

SSLKEY:=$(TESTOUT)/$(SSLSRV)/$(SSLSRV).key
SSLCERT:=$(TESTOUT)/$(SSLSRV)/$(SSLSRV).cert
testssl: $(BINOUT)/$(SSLSRV) $(SSLCERT)
	@echo -n "Testing $(<F) (press Ctrl-C to stop): "
	env LD_LIBRARY_PATH=$(LIBOUT) $< -c $(SSLCERT) -k $(SSLKEY)

$(SSLCERT) $(SSLKEY): $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	openssl req -utf8 -batch -nodes -out $(SSLCERT) -keyout $(SSLKEY) -x509 -new

VALGRIND:=valgrind
# --track-origins=yes
VALGRINDOPTS:=--tool=memcheck --leak-check=full --trace-children=yes --show-reachable=yes --error-exitcode=1 -v --track-fds=yes
hardtest: $(TAGS) $(BINS) $(LIBS) $(TESTBINS)
	env LD_LIBRARY_PATH=.out/lib $(VALGRIND) $(VALGRINDOPTS) $(BINOUT)/$(ARCHDETECT)
ifndef LIBTORQUE_WITHOUT_ADNS
	env LD_LIBRARY_PATH=.out/lib $(VALGRIND) $(VALGRINDOPTS) $(BINOUT)/$(TORQUEHOST) localhost
endif

$(LIBOUT)/$(TORQUESOL): $(LIBOUT)/$(TORQUEREAL)
	@mkdir -p $(@D)
	ln -fsn $(notdir $<) $@

$(LIBOUT)/$(TORQUESOR): $(LIBOUT)/$(TORQUEREAL)
	@mkdir -p $(@D)
	ln -fsn $(notdir $<) $@

$(LIBOUT)/$(TORQUEREAL): $(TORQUEOBJ)
	@mkdir -p $(@D)
	$(CC) $(TORQUECFLAGS) -o $@ $^ $(TORQUELFLAGS)

$(LIBOUT)/$(TORQUESTAT): $(TORQUEOBJ)
	@mkdir -p $(@D)
	ar rcs $@ $^

$(BINOUT)/$(ARCHDETECT): $(ARCHDETECTOBJ) $(LIBS)
	@mkdir -p $(@D)
	$(CC) $(ARCHDETECTCFLAGS) -o $@ $(ARCHDETECTOBJ) $(ARCHDETECTLFLAGS)

$(BINOUT)/$(TORQUEHOST): $(TORQUEHOSTOBJ) $(LIBS)
	@mkdir -p $(@D)
	$(CC) $(TORQUEHOSTCFLAGS) -o $@ $(TORQUEHOSTOBJ) $(TORQUEHOSTLFLAGS)

# The .o files generated for $(TESTBINS) get removed post-build due to their
# status as "intermediate files". The following directive precludes said
# operation, should it be necessary or desirable:
#.SECONDARY: $(addsuffix .o,$(addprefix $(OUT)/$(TOOLDIR)/testing/,$(TESTBINS)))

$(BINOUT)/libev-%: $(OUT)/$(TOOLDIR)/libev/%.o
	@mkdir -p $(@D)
	$(CC) $(TESTBINCFLAGS) -o $@ $< $(EVTESTBINLFLAGS)

$(BINOUT)/%: $(OUT)/$(TOOLDIR)/testing/%.o $(LIBS)
	@mkdir -p $(@D)
	$(CC) $(TESTBINCFLAGS) -o $@ $< $(TESTBINLFLAGS)

######################################################################
# SPECIAL CASE AREA BEGINS
#
# libev doesn't work well with all our warning flags
$(OUT)/tools/libev/signalrx.o: tools/libev/signalrx.c $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fno-strict-aliasing -c $< -o $@
#
# SPECIAL CASE AREA ENDS
######################################################################

# Generic rules
VFLAGS:=-DTORQUE_VERSIONSTR="\"$(MAJORVER).$(MINORVER).$(RELEASEVER)\""
$(OUT)/%.o: %.c $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(CC) $(VFLAGS) $(CFLAGS) -c $< -o $@

# Assemble only, sometimes useful for close-in optimization
$(OUT)/%.s: %.c $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fverbose-asm -S $< -o $@

# Preprocess only, sometimes useful for debugging
$(OUT)/%.i: %.c $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -E $< -o $@

# Should the network be inaccessible, and local copies are installed, try:
#DOC2MANXSL?=--nonet
$(OUT)/%.3torque: %.xml $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(XSLTPROC) --writesubtree $(@D) -o $@ $(DOC2MANXSL) $<

$(OUT)/%.1torque: %.xml $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(XSLTPROC) --writesubtree $(@D) -o $@ $(DOC2MANXSL) $<

$(OUT)/%.pdf: %.tex %.bib $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(LATEX) -output-directory $(@D) $<
	$(BIBTEX) $(basename $@)
	$(LATEX) -output-directory $(@D) $<
	$(LATEX) -output-directory $(@D) $<

$(OUT)/%.svg: %.dot $(PRETTYDOT) $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(DOT) -Tsvg $< | $(XSLTPROC) -o $@ $(PRETTYDOT) -

$(TAGS): $(SRC) $(CINC) $(MAKEFILE_LIST)
	@mkdir -p $(@D)
	$(TAGBIN) -f $@ $^

clean:
	rm -rf $(OUT)

mrproper: clean
	rm -rf $(TAGS)

install: test unsafe-install

unsafe-install: $(LIBS) $(BINS) $(PKGCONFIG) $(DOCS)
	@mkdir -p $(PREFIX)/lib
	$(INSTALL) -m 0644 $(realpath $(REALSOS) $(LIBOUT)/$(TORQUESTAT)) $(PREFIX)/lib
	@(cd $(PREFIX)/lib ; ln -sf $(notdir $(REALSOS) $(TORQUESOL)))
ifeq ($(UNAME),FreeBSD)
	@(cd $(PREFIX)/lib ; ln -sf $(notdir $(REALSOS) $(TORQUESOR)))
endif
	@mkdir -p $(PREFIX)/bin
	@$(INSTALL) $(BINS) $(PREFIX)/bin
	@mkdir -p $(PREFIX)/include/lib$(TORQUE)
	@$(INSTALL) -m 0644 $(INCINSTALL) $(PREFIX)/include/lib$(TORQUE)/
	@[ ! -d $(PREFIX)/lib/pkgconfig ] || \
		$(INSTALL) -m 0644 $(PKGCONFIG) $(PREFIX)/lib/pkgconfig
	@mkdir -p $(DOCPREFIX)/man1 $(DOCPREFIX)/man3
	@$(INSTALL) -m 0644 $(MAN1OBJ) $(DOCPREFIX)/man1
	@$(INSTALL) -m 0644 $(MAN3OBJ) $(DOCPREFIX)/man3
	@echo "Running $(LDCONFIG) $(PREFIX)/lib..." && $(LDCONFIG) $(PREFIX)/lib
	@echo "Running $(MANBIN) $(DOCPREFIX)..." && $(MANBIN) $(DOCPREFIX)

deinstall:
	rm -f $(addprefix $(DOCPREFIX)/man3/,$(notdir $(MAN3OBJ)))
	rm -f $(addprefix $(DOCPREFIX)/man1/,$(notdir $(MAN1OBJ)))
	rm -rf $(PREFIX)/include/lib$(TORQUE)
	rm -f $(PREFIX)/lib/pkgconfig/$(notdir $(PKGCONFIG))
	rm -f $(addprefix $(PREFIX)/bin/,$(notdir $(BINS)))
	rm -f $(addprefix $(PREFIX)/lib/,$(notdir $(LIBS)))
	rm -f $(addprefix $(PREFIX)/lib/,$(notdir $(REALSOS) $(TORQUESOL)))
	@echo "Running $(LDCONFIG) $(PREFIX)/lib..." && $(LDCONFIG) $(PREFIX)/lib
	@echo "Running $(MANBIN) $(DOCPREFIX)..." && $(MANBIN) $(DOCPREFIX)
