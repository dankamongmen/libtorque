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
# Avoid unnecessary uses of 'pwd'; absolute paths aren't as robust as relative
# paths against overlong total path names.
OBJOUT:=.out

.DELETE_ON_ERROR:

.PHONY: all test clean install unsafe-install deinstall

.DEFAULT: all

TAGS:=.tags

all: $(TAGS) test

test: $(TAGS)

$(TAGS): $(MAKEFILE_LIST)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(TAGBIN) -f $@ $^

clean:
	rm -rf $(OBJOUT) $(TAGS)

install: test unsafe-install

unsafe-install:

deinstall:
