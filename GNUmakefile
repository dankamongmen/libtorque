.DELETE_ON_ERROR:

.PHONY: all test clean install unsafe-install deinstall

.DEFAULT: all

# Don't run shell commands over and over; cache commonly-used results here.
UNAME:=$(shell uname)

# Variables defined with ?= can be inherited from the environment, and thus
# specified. Provide the defaults here. Document these in the README.
PREFIX?=/usr/local
ifeq ($(UNAME),Linux)
CC?=gcc-4.4
else
ifeq ($(UNAME),FreeBSD)
CC?=gcc44
endif
endif

all: test

test:

clean:

install: test unsafe-install

unsafe-install:

deinstall:
