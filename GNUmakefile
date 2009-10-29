.DELETE_ON_ERROR:
.PHONY: all test clean install unsafe-install deinstall
.DEFAULT: all

# Shared object versioning. MAJORVER will become 1 upon the first stable
# release, and at that point changes only when the API changes. The minor
# and release versions change more arbitrarily. Only MAJORVER is in the soname.
MAJORVER:=0
MINORVER:=0
RELEASEVER:=1

# Don't run shell commands unnecessarily. Cache commonly-used results here.
UNAME:=$(shell uname)

######################################################################
# USER SPECIFICATION AREA BEGINS
#
# Variables defined with ?= can be inherited from the environment, and thus
# specified. Provide the defaults here. Document these in the README.
PREFIX?=/usr/local

# Some systems don't install exuberant-ctags as 'ctags'. Some people use etags.
TAGBIN?=$(shell which exctags 2> /dev/null || echo ctags)

# We compile for the host µ-architecture/ISA, providing the "native" option to
# gcc's -march and -mtune. If you don't have gcc 4.3 or greater, you'll need to
# define appropriate march and mtune values for your system (see gcc's
# "Submodel Options" info page). Libraries intended to be run on arbitrary x86
# hardware must be built with MARCH defined as "generic", and MTUNE unset.
MARCH?=native
ifneq ($(MARCH),generic)
MTUNE?=native
endif

# System-specific, specification-optional variables.
ifeq ($(UNAME),Linux)
CC?=$(shell which gcc-4.4 2> /dev/null || echo gcc)
else
ifeq ($(UNAME),FreeBSD)
CC?=$(shell which gcc44 2> /dev/null || echo gcc)
endif
endif
#
# USER SPECIFICATION AREA ENDS
######################################################################

# Unilateral definitions, shielded from the environment (save as components).

# System-specific variables closed to external specification
ifeq ($(UNAME),Linux)
DFLAGS:=-D_FILE_OFFSET_BITS=64
LFLAGS:=-Wl,--warn-shared-textrel
else
ifeq ($(UNAME),FreeBSD)
DFLAGS:=-D_THREAD_SAFE -D_POSIX_PTHREAD_SEMANTICS
endif
endif

INSTALL:=install -v

# Codenames are factored out, to accommodate changing them later.
TORQUE:=torque
ARCHDETECT:=archdetect

# Avoid unnecessary uses of 'pwd'; absolute paths aren't as robust as relative
# paths against overlong total path names.
OBJOUT:=.out
SRCDIR:=src
TOOLDIR:=tools
CSRCDIRS:=$(wildcard $(SRCDIR)/*)
TORQUEDIRS:=$(SRCDIR)/lib$(TORQUE)
ARCHDETECTDIRS:=$(SRCDIR)/$(ARCHDETECT)

# Anything that all source->object translations ought dep on. We currently
# include all header files in this list; it'd be nice to refine that FIXME.
TAGS:=.tags
GLOBOBJDEPS:=$(TAGS) $(CINC)

# Simple compositions from here on out
LIBOUT:=$(OBJOUT)/lib
BINOUT:=$(OBJOUT)/bin
TORQUESOL:=lib$(TORQUE).so
TORQUESOR:=$(TORQUESOL).$(MAJORVER)
TORQUEREAL:=$(TORQUESOR).$(MINORVER).$(RELEASEVER)
PKGCONFIG:=$(TOOLDIR)/lib$(TORQUE).pc

# We don't want to have to list all our source files, so discover them based on
# the per-language directory specifications above.
CSRC:=$(shell find $(CSRCDIRS) -type f -name \*.c -print)
CINC:=$(shell find $(CSRCDIRS) -type f -name \*.h -print)
TORQUESRC:=$(foreach dir, $(TORQUEDIRS), $(filter $(dir)/%, $(CSRC)))
TORQUEOBJ:=$(addprefix $(OBJOUT)/,$(TORQUESRC:%.c=%.o))
ARCHDETECTSRC:=$(foreach dir, $(ARCHDETECTDIRS), $(filter $(dir)/%, $(CSRC)))
ARCHDETECTOBJ:=$(addprefix $(OBJOUT)/,$(ARCHDETECTSRC:%.c=%.o))
SRC:=$(CSRC)
BINS:=$(addprefix $(BINOUT)/,$(ARCHDETECT))
LIBS:=$(addprefix $(LIBOUT)/,$(TORQUESOL) $(TORQUESOR))
REALSOS:=$(addprefix $(LIBOUT)/,$(TORQUEREAL))

# Main compilation flags. Define with += to inherit from system-specific flags.
IFLAGS:=-I$(SRCDIR)
MFLAGS:=-fpic -march=$(MARCH)
ifdef MTUNE
MFLAGS+=-mtune=$(MTUNE)
endif
WFLAGS:=-Werror -Wall -W -Wextra -Wmissing-prototypes -Wundef -Wshadow \
        -Wstrict-prototypes -Wmissing-declarations -Wnested-externs \
        -Wsign-compare -Wpointer-arith -Wbad-function-cast -Wcast-qual \
        -Wdeclaration-after-statement -Wfloat-equal -Wpacked -Winvalid-pch \
        -Wdisabled-optimization -Wcast-align -Wformat -Wformat-security \
        -Wold-style-definition -Woverlength-strings -Wwrite-strings -Wpadded \
	-Wstrict-aliasing=2 -Wconversion
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
# -fipa-pta -fipa-cp -ftree-loop-linear -ftree-loop-im -ftree-loop-ivcanon
# The following also require profiling info:
# -fipa-matrix-reorg
# These require -pthread:
# -ftree-parallelize-loops
OFLAGS:=-O2 -fomit-frame-pointer -finline-functions -fdiagnostics-show-option \
	-fvisibility=hidden -fipa-cp -ftree-loop-linear -ftree-loop-im \
	-ftree-loop-ivcanon
DEBUGFLAGS:=-rdynamic -g
CFLAGS:=-pipe -std=gnu99 -pthread $(DFLAGS) $(IFLAGS) $(MFLAGS) $(OFLAGS) $(WFLAGS)
LFLAGS+=-Wl,-O,--default-symver,--enable-new-dtags,--as-needed,--warn-common \
	-Wl,--fatal-warnings,-z,noexecstack,-z,combreloc -lpthread
TORQUECFLAGS:=$(CFLAGS) -shared
TORQUELFLAGS:=$(LFLAGS) -Wl,-soname,$(TORQUESOR)
ARCHDETECTCFLAGS:=$(CFLAGS)
ARCHDETECTLFLAGS:=$(LFLAGS) -L$(LIBOUT) -ltorque

# In addition to the binaries and unit tests, 'all' builds documentation,
# packaging, graphs, and all that kind of crap.
all: test

test: $(BINS) $(LIBS)
	@echo -n "Testing $(ARCHDETECT): "
	env LD_LIBRARY_PATH=$(LIBOUT) $(BINOUT)/$(ARCHDETECT)

$(LIBOUT)/$(TORQUESOL): $(LIBOUT)/$(TORQUEREAL)
	@mkdir -p $(@D)
	ln -fsn $(notdir $<) $@

$(LIBOUT)/$(TORQUESOR): $(LIBOUT)/$(TORQUEREAL)
	@mkdir -p $(@D)
	ln -fsn $(notdir $<) $@

$(LIBOUT)/$(TORQUEREAL): $(TORQUEOBJ)
	@mkdir -p $(@D)
	$(CC) $(TORQUECFLAGS) -o $@ $^ $(TORQUELFLAGS)

$(BINOUT)/$(ARCHDETECT): $(ARCHDETECTOBJ) $(LIBS)
	@mkdir -p $(@D)
	$(CC) $(ARCHDETECTCFLAGS) -o $@ $(ARCHDETECTOBJ) $(ARCHDETECTLFLAGS)

$(OBJOUT)/%.o: %.c $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble only, sometimes useful for close-in optimization
$(OBJOUT)/%.s: %.c $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -S $< -o $@

# Preprocess only, sometimes useful for debugging
$(OBJOUT)/%.i: %.c $(GLOBOBJDEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -E $< -o $@

# Having TAGS dep on the involved makefiles -- and including TAGS in
# GLOBOBJDEPS -- means that a makefile change forces global rebuilding, which
# seems a desirable goal anyway.
$(TAGS): $(SRC) $(CINC) $(MAKEFILE_LIST)
	@mkdir -p $(@D)
	$(TAGBIN) -f $@ $^

clean:
	@rm -rfv $(OBJOUT) $(TAGS)

install: test unsafe-install

unsafe-install: $(LIBS) $(PKGCONFIG)
	@mkdir -p $(PREFIX)/lib
	@$(INSTALL) $(realpath $(REALSOS)) $(PREFIX)/lib
	@[ ! -d $(PREFIX)/lib/pkgconfig ] || \
		$(INSTALL) $(PKGCONFIG) $(PREFIX)/lib/pkgconfig
	@echo "Running ldconfig..." && ldconfig

deinstall:
	@rm -fv $(PREFIX)/lib/pkgconfig/$(notdir $(PKGCONFIG))
	@rm -rfv $(addprefix $(PREFIX)/lib/,$(notdir $(LIBS)))
	@rm -rfv $(addprefix $(PREFIX)/lib/,$(notdir $(REALSOS)))
	@echo "Running ldconfig..." && ldconfig
