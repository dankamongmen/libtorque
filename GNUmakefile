.DELETE_ON_ERROR:
.PHONY: all test testarchdetect hardtest testssl docs clean mrproper flow \
	install unsafe-install deinstall
.DEFAULT_GOAL:=test

# Shared object versioning. MAJORVER will become 1 upon the first stable
# release, and at that point changes only when the API changes. The minor
# and release versions change more arbitrarily. Only MAJORVER is in the soname.
MAJORVER:=0
MINORVER:=0
RELEASEVER:=1
DFLAGS+=-DLIBTORQUE_VERSIONSTR="\"$(MAJORVER).$(MINORVER).$(RELEASEVER)\""

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
TAGBIN?=$(shell which exctags 2> /dev/null || echo ctags)

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
DFLAGS+=$(shell pkg-config --cflags openssl)
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

# Any old XSLT processor ought do, but you might need change the invocation.
XSLTPROC?=$(shell (which xsltproc || echo xsltproc) 2> /dev/null)
# This can be a URL; it's the docbook-to-manpage XSL
#DOC2MANXSL?=--nonet /usr/share/xml/docbook/stylesheet/docbook-xsl/manpages/docbook.xsl

#
# USER SPECIFICATION AREA ENDS
######################################################################

# Unilateral definitions, shielded from the environment (save as components).

# System-specific variables closed to external specification
ifeq ($(UNAME),Linux)
DFLAGS+=-DLIBTORQUE_LINUX -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
LFLAGS+=-Wl,--warn-shared-textrel
MANBIN:=mandb
LDCONFIG:=ldconfig -n
else
ifeq ($(UNAME),FreeBSD)
DFLAGS+=-DLIBTORQUE_FREEBSD
MT_DFLAGS:=-D_THREAD_SAFE -D_POSIX_PTHREAD_SEMANTICS
MANBIN:=makewhatis
LDCONFIG:=ldconfig -m
endif
endif

INSTALL:=install -v

# Codenames are factored out, to accommodate changing them later.
ARCHDETECT:=archdetect
SSLSRV:=torquessl
TORQUE:=torque

# Avoid unnecessary uses of 'pwd'; absolute paths aren't as robust as relative
# paths against overlong total path names.
OUT:=.out
SRCDIR:=src
TOOLDIR:=tools
MANDIR:=doc/man
CSRCDIRS:=$(wildcard $(SRCDIR)/*)
ARCHDETECTDIRS:=$(SRCDIR)/$(ARCHDETECT)
SSLSRVDIRS:=$(SRCDIR)/$(SSLSRV)
TORQUEDIRS:=$(SRCDIR)/lib$(TORQUE)

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
SSLSRVSRC:=$(foreach dir, $(SSLSRVDIRS), $(filter $(dir)/%, $(CSRC)))
SSLSRVOBJ:=$(addprefix $(OUT)/,$(SSLSRVSRC:%.c=%.o))
TORQUESRC:=$(foreach dir, $(TORQUEDIRS), $(filter $(dir)/%, $(CSRC)))
TORQUEOBJ:=$(addprefix $(OUT)/,$(TORQUESRC:%.c=%.o))
SRC:=$(CSRC)
TESTBINS:=$(addprefix $(BINOUT)/,echoserver signalrx signaltx torquessl) # FIXME autodiscover
ifndef LIBTORQUE_WITHOUT_EV
TESTBINS+=$(addprefix $(BINOUT)/libev-,signalrx)
endif
BINS:=$(addprefix $(BINOUT)/,$(ARCHDETECT))
LIBS:=$(addprefix $(LIBOUT)/,$(TORQUESOL) $(TORQUESOR) $(TORQUESTAT))
REALSOS:=$(addprefix $(LIBOUT)/,$(TORQUEREAL))

# Documentation processing
MAN1SRC:=$(shell find $(MANDIR)/man1/ -type f -print)
MAN3SRC:=$(shell find $(MANDIR)/man3/ -type f -print)
MAN1OBJ:=$(addprefix $(OUT)/,$(MAN1SRC:%.xml=%.1))
MAN3OBJ:=$(addprefix $(OUT)/,$(MAN3SRC:%.xml=%.3))
DOCS:=$(MAN1OBJ) $(MAN3OBJ)
INCINSTALL:=$(addprefix $(SRCDIR)/lib$(TORQUE)/,lib$(TORQUE).h ssl/ssl.h)
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
WFLAGS+=-Werror -Wall -W -Wextra -Wmissing-prototypes -Wundef -Wshadow \
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
	-ftree-loop-ivcanon -fno-common
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
TESTBINCFLAGS:=$(MT_CFLAGS)
TESTBINLFLAGS:=$(LFLAGS) -Wl,-R$(LIBOUT) -L$(LIBOUT) -ltorque
EVTESTBINLFLAGS:=$(LFLAGS) -lev

flow:
	cflow $(IFLAGS) $(DFLAGS) $(TORQUESRC) 2>/dev/null

# In addition to the binaries and unit tests, 'all' builds documentation,
# packaging, graphs, and all that kind of crap.
all: docs test

docs: $(TAGS) $(DOCS)

test: $(TAGS) $(BINS) $(LIBS) $(TESTBINS) testarchdetect

testarchdetect: $(BINOUT)/$(ARCHDETECT)
	@echo -n "Testing $(<F): "
	env LD_LIBRARY_PATH=$(LIBOUT) $<

SSLKEY:=$(TESTOUT)/$(SSLSRV)/$(SSLSRV).key
SSLCERT:=$(TESTOUT)/$(SSLSRV)/$(SSLSRV).cert
testssl: $(BINOUT)/$(SSLSRV) $(SSLCERT)
	@echo -n "Testing $(<F) (press Ctrl-C to stop): "
	env LD_LIBRARY_PATH=$(LIBOUT) $< -c $(SSLCERT) -k $(SSLKEY)

$(SSLCERT) $(SSLKEY): $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	openssl req -utf8 -batch -nodes -out $(SSLCERT) -keyout $(SSLKEY) -x509 -new

VALGRIND:=valgrind
VALGRINDOPTS:=--tool=memcheck --leak-check=full --trace-children=yes --show-reachable=yes --error-exitcode=1 -v --track-origins=yes
hardtest: $(TAGS) $(BINS) $(LIBS) $(TESTBINS)
	env LD_LIBRARY_PATH=.out/lib $(VALGRIND) $(VALGRINDOPTS) $(BINOUT)/$(ARCHDETECT)

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

$(BINOUT)/$(SSLSRV): $(OUT)/tools/testing/$(SSLSRV).o $(LIBS)
	@mkdir -p $(@D)
	$(CC) $(TESTBINCFLAGS) -o $@ $< $(TESTBINLFLAGS)

# The .o files generated for $(TESTBINS) get removed post-build due to their
# status as "intermediate files". The following directive precludes said
# operation, should it be necessary or desirable:
#.SECONDARY: $(addsuffix .o,$(addprefix $(OUT)/tools/testing/,$(TESTBINS)))

$(BINOUT)/libev-%: $(OUT)/tools/libev/%.o
	@mkdir -p $(@D)
	$(CC) $(TESTBINCFLAGS) -o $@ $< $(EVTESTBINLFLAGS)

$(BINOUT)/%: $(OUT)/tools/testing/%.o $(LIBS)
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
$(OUT)/%.o: %.c $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

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
$(OUT)/%.3: %.xml $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(XSLTPROC) -o $@ $(DOC2MANXSL) $<

$(OUT)/%.1: %.xml $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(XSLTPROC) -o $@ $(DOC2MANXSL) $<

$(TAGS): $(SRC) $(CINC) $(MAKEFILE_LIST)
	@mkdir -p $(@D)
	$(TAGBIN) -f $@ $^

clean:
	@rm -rfv $(OUT)

mrproper: clean
	@rm -rfv $(TAGS)

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
	@echo "Running $(LDCONFIG)..." && $(LDCONFIG) $(PREFIX)/lib
	@echo "Running $(MANBIN)..." && $(MANBIN) $(DOCPREFIX)

deinstall:
	@rm -fv $(addprefix $(DOCPREFIX)/man3/,$(notdir $(MAN3OBJ)))
	@rm -fv $(addprefix $(DOCPREFIX)/man1/,$(notdir $(MAN1OBJ)))
	@rm -rfv $(PREFIX)/include/lib$(TORQUE)
	@rm -fv $(PREFIX)/lib/pkgconfig/$(notdir $(PKGCONFIG))
	@rm -fv $(addprefix $(PREFIX)/bin/,$(notdir $(BINS)))
	@rm -fv $(addprefix $(PREFIX)/lib/,$(notdir $(LIBS)))
	@rm -fv $(addprefix $(PREFIX)/lib/,$(notdir $(REALSOS) $(TORQUESOL)))
	@echo "Running $(LDCONFIG)..." && $(LDCONFIG) $(PREFIX)/lib
	@echo "Running $(MANBIN)..." && $(MANBIN) $(DOCPREFIX)
