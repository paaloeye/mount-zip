# Copyright 2021 Google LLC
# Copyright 2008-2021 Alexander Galanin <al@galanin.nnov.ru>
# http://galanin.nnov.ru/~al
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

PROJECT_NAME = mount-zip
OUT = out
ABS_OUT = $(abspath $(OUT))
DEST = $(OUT)/$(PROJECT_NAME)
PKG_CONFIG ?= pkg-config

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

PROJECT_CXXFLAGS = -std=c++20 -Wall -Wextra -Wno-nullability-extension \
                   -Wno-sign-compare -Wno-missing-field-initializers \
                   -pedantic -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64

FUSE_MAJOR_VERSION ?= 3

ifeq ($(FUSE_MAJOR_VERSION), 3)
DEPS = fuse3
PROJECT_CXXFLAGS += -DFUSE_USE_VERSION=30
else ifeq ($(FUSE_MAJOR_VERSION), 2)
DEPS = fuse
PROJECT_CXXFLAGS += -DFUSE_USE_VERSION=26
endif

DEPS += libzip icu-uc icu-i18n

# On macOS, icu4c is keg-only (Homebrew does not symlink it into the default
# search path because macOS ships its own libicucore). Override PKG_CONFIG to
# carry the pinned icu4c path inline so every pkg-config call in this make and
# all sub-makes resolves the correct version regardless of what the shell
# environment (e.g. pkgx) may have injected.

ifeq ($(shell uname -s),Darwin)
  BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
  ifneq ($(BREW_PREFIX),)
    PKG_CONFIG = env PKG_CONFIG_PATH="$(BREW_PREFIX)/opt/icu4c/lib/pkgconfig" pkg-config
    CPPFLAGS += -I$(BREW_PREFIX)/opt/boost/include
    export PKG_CONFIG
    export CPPFLAGS
  endif
endif

PROJECT_CXXFLAGS += $(shell $(PKG_CONFIG) --cflags $(DEPS))
PROJECT_LDFLAGS = -L$(OUT) -lmountzip $(shell $(PKG_CONFIG) --libs $(DEPS))

ifeq ($(DEBUG), 1)
PROJECT_CXXFLAGS += -O0 -g
else
PROJECT_CXXFLAGS += -O2 -DNDEBUG
endif

ifeq ($(ASAN), 1)
PROJECT_CXXFLAGS += -fsanitize=address
PROJECT_LDFLAGS += -fsanitize=address
endif

ifeq ($(UBSAN), 1)
PROJECT_CXXFLAGS += -fsanitize=undefined
PROJECT_LDFLAGS += -fsanitize=undefined
endif

LIB = $(OUT)/libmountzip.a
SOURCES = mount-zip.cc
OBJECTS = $(addprefix $(OUT)/,$(SOURCES:.cc=.o))
MAN = $(PROJECT_NAME).1
INSTALL = install

all: $(DEST)

doc: $(MAN)
	@if [ -z "$(QUIET)" ]; then man -l $(MAN); fi

release:
	python3 release.py $(VERSION)

$(DEST): $(OBJECTS) $(LIB)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(PROJECT_LDFLAGS) -o $@

$(OUT)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) -Ilib -c $(CPPFLAGS) $(PROJECT_CXXFLAGS) $(CXXFLAGS) $< -o $@ -MMD -MP -MF $(@:.o=.d)

ifneq ($(filter clean,$(MAKECMDGOALS)),)
else
-include $(OBJECTS:.o=.d)
endif

$(LIB):
	$(MAKE) -C lib OUT=$(ABS_OUT) ASAN=$(ASAN) UBSAN=$(UBSAN) DEBUG=$(DEBUG)

clean:
	rm -rf $(OUT)
	$(MAKE) -C tests clean OUT=$(ABS_OUT)
	$(MAKE) -C lib clean OUT=$(ABS_OUT)

$(MAN): README.md
	pandoc $< -s -t man | \
	sed -e 's/^\.IP \\\[bu\]/.PD 0\n.IP \\\[bu\]/g' \
	    -e 's/^\.SH/.PD\n.SH/g' \
	    -e 's/^\.SS/.PD\n.SS/g' \
	    -e 's/^\.PP/.PD\n.PP/g' \
	    -e 's/^\.TP/.PD\n.TP/g' > $@

install: $(DEST)
	$(INSTALL) -D "$(DEST)" "$(DESTDIR)$(BINDIR)/mount-zip"
	$(INSTALL) -D -m 644 $(MAN) "$(DESTDIR)$(MANDIR)/$(MAN)"

install-strip: $(DEST)
	$(INSTALL) -D -s "$(DEST)" "$(DESTDIR)$(BINDIR)/mount-zip"
	$(INSTALL) -D -m 644 $(MAN) "$(DESTDIR)$(MANDIR)/$(MAN)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/mount-zip" "$(DESTDIR)$(MANDIR)/$(MAN)"

debug:
	$(MAKE) DEBUG=1 all

check: $(DEST)
	$(MAKE) -C tests check OUT=$(ABS_OUT) ASAN=$(ASAN) UBSAN=$(UBSAN)

check-fast: $(DEST)
	$(MAKE) -C tests check-fast OUT=$(ABS_OUT) ASAN=$(ASAN) UBSAN=$(UBSAN)

valgrind: $(DEST)
	$(MAKE) -C tests valgrind OUT=$(ABS_OUT)

.PHONY: all doc debug clean check-fast install install-strip release test uninstall check valgrind $(LIB)

