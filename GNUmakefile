.DELETE_ON_ERROR:
.PHONY: all test clean install unsafe-install deinstall
.DEFAULT: all

# Shared object versioning. MAJORVER will become 1 upon the first stable
# release, and at that point changes only when the API changes. The minor
# and release versions change more arbitrarily. Only MAJORVER is in the soname.
MAJORVER:=0
MINORVER:=0
RELEASEVER:=1
TORQUEVER:=$(MAJORVER).$(MINORVER).$(RELEASEVER)

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
CC?=gcc-4.4
else
ifeq ($(UNAME),FreeBSD)
CC?=gcc44
endif
endif
#
# USER SPECIFICATION AREA ENDS
######################################################################

# Unilateral definitions, shielded from the environment (save as components).

# System-specific variables closed to external specification
ifeq ($(UNAME),Linux)
READLINK:=readlink -f
DFLAGS:=-D_FILE_OFFSET_BITS=64
else
ifeq ($(UNAME),FreeBSD)
READLINK:=realpath
DFLAGS:=-D_THREAD_SAFE -D_POSIX_PTHREAD_SEMANTICS
endif
endif

# Codenames are factored out, to accommodate changing them later.
TORQUE:=torque

# Avoid unnecessary uses of 'pwd'; absolute paths aren't as robust as relative
# paths against overlong total path names.
OBJOUT:=.out
SRCDIR:=src
CSRCDIRS:=$(wildcard $(SRCDIR)/*)

# Anything that all source->object translations ought dep on. We currently
# include all header files in this list; it'd be nice to refine that FIXME.
GLOBOBJDEPS:=$(TAGS) $(CINC)

# Simple compositions from here on out
TORQUELIB:=lib$(TORQUE).so
TORQUEDIRS:=$(SRCDIR)/lib$(TORQUE)
LIBOUT:=$(OBJOUT)/lib

# We don't want to have to list all our source files, so discover them based on
# the per-language directory specifications above.
CSRC:=$(shell find $(CSRCDIRS) -type f -name \*.c -print)
CINC:=$(shell find $(CSRCDIRS) -type f -name \*.h -print)
TORQUESRC:=$(foreach dir, $(TORQUEDIRS), $(filter $(dir)/%, $(CSRC)))
TORQUEOBJ:=$(addprefix $(OBJOUT)/,$(TORQUESRC:%.c=%.o))
SRC:=$(CSRC)
LIBS:=$(addprefix $(LIBOUT)/,$(TORQUELIB).$(MAJORVER))

# Main compilation flags. Define with += to inherit from system-specific flags.
IFLAGS:=-I$(SRCDIR)
MFLAGS:=-march=$(MARCH)
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
# -ftree-dominator-opts -ftree-dse -ftree-fre -ftree-sra -ftree-ter \
# -funit-at-a-time, "and -fomit-frame-pointer on machines where it doesn't
# interfere with debugging."
# We add the following with -O2 (taken from gcc 4.3 docs)
# -fthread-jumps -falign-functions  -falign-jumps -falign-loops -falign-labels \
# -fcaller-saves -fcrossjumping -fcse-follow-jumps  -fcse-skip-blocks \
# -fdelete-null-pointer-checks -fexpensive-optimizations -fgcse -fgcse-lm \
# -foptimize-sibling-calls -fpeephole2 -fregmove -freorder-blocks \
# -freorder-functions -frerun-cse-after-loop -fsched-interblock -fsched-spec \
# -fschedule-insns -fschedule-insns2 -fstrict-aliasing -fstrict-overflow \
# -ftree-pre -ftree-vrp
OFLAGS:=-O2 -fomit-frame-pointer -finline-functions -fdiagnostics-show-option
DEBUGFLAGS:=-rdynamic -g
CFLAGS:=-pipe -std=gnu99 -pthread $(DFLAGS) $(IFLAGS) $(MFLAGS) $(OFLAGS) $(WFLAGS)
LFLAGS:=-Wl,-O,--default-symver,--enable-new-dtags,--as-needed,--warn-common \
	-Wl,--fatal-warnings,--warn-shared-textrel,-z,noexecstack,-z,combreloc \
	-lpthread
TORQUECFLAGS:=$(CFLAGS) -shared
TORQUELFLAGS:=$(LFLAGS)

TAGS:=.tags

# In addition to the binaries and unit tests, 'all' builds documentation,
# packaging, graphs, and all that kind of crap.
all: test

test: $(LIBS) $(TAGS)

$(LIBOUT)/$(TORQUELIB).$(MAJORVER): $(LIBOUT)/$(TORQUELIB).$(TORQUEVER)
	@[ -d $(@D) ] || mkdir -p $(@D)
	ln -fsn $(shell $(READLINK) $<) $@

$(LIBOUT)/$(TORQUELIB).$(TORQUEVER): $(TORQUEOBJ)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(TORQUECFLAGS) -o $@ $^ $(TORQUELFLAGS)

$(OBJOUT)/%.o: %.c $(GLOBOBJDEPS)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble only, sometimes useful for close-in optimization
$(OBJOUT)/%.s: %.c $(GLOBOBJDEPS)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(CFLAGS) -S $< -o $@

# Preprocess only, sometimes useful for debugging
$(OBJOUT)/%.i: %.c $(GLOBOBJDEPS)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(CFLAGS) -E $< -o $@

# Having TAGS dep on the involved makefiles -- and including TAGS in
# GLOBOBJDEPS -- means that a makefile change forces global rebuilding, which
# seems a desirable goal anyway.
$(TAGS): $(SRC) $(MAKEFILE_LIST)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(TAGBIN) -f $@ $^

clean:
	rm -rf $(OBJOUT) $(TAGS)

install: test unsafe-install

unsafe-install:

deinstall:
