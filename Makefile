# Makefile for Flat's Packages
# SPDX-License-Identifier: AGPL-3.0-only

CC           ?= cc
CFLAGS       ?= -O2 -Wall -Wextra
PREFIX       ?= /usr/local
INSTALL_ARGS ?=

all: flatspkg install-tool

flatspkg: flatspkg.c
	$(CC) $(CFLAGS) -o $@ $<

install-tool: install.c
	$(CC) $(CFLAGS) -o install $<

check: flatspkg
	BIN=./flatspkg bash ./tests/test.sh

install: all
	./install $(INSTALL_ARGS)

install-user: all
	./install --user

install-system: all
	./install --system

install-prefix: all
	./install --prefix=$(PREFIX)

uninstall:
	rm -f "$(PREFIX)/bin/flatspkg"
	rm -f "$(PREFIX)/share/man/man1/flatspkg.1"
	rm -f "$(PREFIX)/share/bash-completion/completions/flatspkg"
	rm -f "$(PREFIX)/share/zsh/site-functions/_flatspkg"
	rm -f "$(PREFIX)/share/fish/vendor_completions.d/flatspkg.fish"

clean:
	rm -f flatspkg install

.PHONY: all check install install-user install-system install-prefix \
        uninstall clean