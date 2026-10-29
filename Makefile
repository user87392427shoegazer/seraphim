# seraphim -- a Wayland window manager whose desk is an infinite canvas and
# whose policy is AngelScript.
#
#   make deps      fetch and build the AngelScript SDK (needs git)
#   make           build ./seraphim and ./seraphimctl
#   make test      build and run the headless WM-logic tests (no wlroots)
#   make install   install the binary, the script and the session file
#   make clean

include config.mk

CXXFLAGS  += -std=c++17 -O2 $(if $(DEBUG),-g) -Wall -Wextra -Wno-unused-parameter -D_POSIX_C_SOURCE=200809L

# For the crash handler: it walks the frame chain and prints return addresses,
# which only map back to source lines with `addr2line` when the binary keeps
# frame pointers and is not position independent.
CXXFLAGS  += -fno-omit-frame-pointer -no-pie -ffunction-sections -fdata-sections

# -MMD -MP writes a .d file per object listing the headers it read, so editing
# config/config_keys.h actually rebuilds everything that includes it. Without
# this, changing a config value silently produced a binary built from the old
# numbers.
DEPFLAGS  = -MMD -MP
CPPFLAGS  += -Iinclude -Iconfig

AS_DIR       = $(ANGELSCRIPT_DIR)
AS_INC       = $(AS_DIR)/sdk/angelscript/include $(AS_DIR)/sdk/add_on
AS_LIB       = $(AS_DIR)/sdk/angelscript/lib/libangelscript.a
AS_STDSTRING = $(AS_DIR)/sdk/add_on/scriptstdstring/scriptstdstring.cpp
AS_ARRAY     = $(AS_DIR)/sdk/add_on/scriptarray/scriptarray.cpp
AS_CPPFLAGS  = -I$(AS_DIR)/sdk/angelscript/include -I$(AS_DIR)/sdk/add_on

WLR_CFLAGS = $(shell $(PKG_CONFIG) --cflags $(WLR_PC) 2>/dev/null)
WLR_LIBS   = $(shell $(PKG_CONFIG) --libs $(WLR_PC) 2>/dev/null)
DEPS_CFLAGS = $(shell $(PKG_CONFIG) --cflags $(DEPS_PKGS) 2>/dev/null)
DEPS_LIBS   = $(shell $(PKG_CONFIG) --libs $(DEPS_PKGS) 2>/dev/null)

# Grows a --version banner and the project name into the binary. WLR_PC is the
# pkg-config name of the wlroots the host was built against, so the banner can
# say which one it is.
CPPFLAGS += -DSERAPHIM_VERSION=\"$(VERSION)\" -DWLR_PC=\"$(WLR_PC)\"

# The policy engine: model, camera, borders and the AngelScript bridge. None
# of it touches wlroots, which is why the tests can link it on its own.
CORE_SRC = src/canvas.cpp src/borders.cpp src/world.cpp src/script.cpp src/ipc.cpp
HOST_SRC = src/main.cpp

CORE_OBJ = $(patsubst src/%.cpp,build/%.o,$(CORE_SRC))
HOST_OBJ = $(patsubst src/%.cpp,build/%.o,$(HOST_SRC))
AS_OBJ   = build/angelscript/scriptstdstring.o build/angelscript/scriptarray.o

# wlroots' public headers include generated protocol headers such as
# wlr-layer-shell-unstable-v1-protocol.h. The wlroots packages ship neither
# those headers nor, on Alpine, any of the XML: including wlr_layer_shell_v1.h
# fails outright. protocols/ vendors the XML (wlroots MIT, wayland-protocols
# MIT) and the headers are generated into build/protocols at build time, which
# needs wayland-scanner: wayland-dev on Alpine, libwayland-bin on Debian.
PROTO_DIR    = build/protocols
PROTO_SRC    = $(wildcard protocols/*.xml)
PROTO_HDR    = $(patsubst protocols/%.xml,$(PROTO_DIR)/%-protocol.h,$(PROTO_SRC))
PROTO_ENUM   = $(PROTO_DIR)/wayland-protocols/cursor-shape-v1-enum.h \
               $(PROTO_DIR)/wayland-protocols/pointer-constraints-unstable-v1-enum.h
PROTO_CFLAGS = -I$(PROTO_DIR)
WL_SCANNER  ?= wayland-scanner

all: seraphim seraphimctl

seraphim: $(CORE_OBJ) $(HOST_OBJ) $(AS_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(CORE_OBJ) $(HOST_OBJ) $(AS_OBJ) $(AS_LIB) \
		$(DEPS_LIBS) $(WLR_LIBS) -lpthread -lm -Wl,--gc-sections

seraphimctl: tools/seraphimctl.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# The core is built without wlroots flags so `make test` works on a machine
# with no wlroots installed.
build/%.o: src/%.cpp | build
	$(CXX) $(DEPFLAGS) $(CPPFLAGS) $(AS_CPPFLAGS) $(CXXFLAGS) $(WLR_CFLAGS) $(DEPS_CFLAGS) -c $< -o $@

# The host is the only target that reads wlroots, so it is the only one that
# needs the generated protocol headers.
build/main.o: src/main.cpp $(PROTO_HDR) $(PROTO_ENUM) | build
	$(CXX) $(DEPFLAGS) $(CPPFLAGS) $(AS_CPPFLAGS) $(PROTO_CFLAGS) $(CXXFLAGS) \
		$(WLR_CFLAGS) $(DEPS_CFLAGS) -c $< -o $@

$(PROTO_DIR)/%-protocol.h: protocols/%.xml | $(PROTO_DIR)
	@command -v $(WL_SCANNER) >/dev/null 2>&1 || { \
		echo "seraphim: $(WL_SCANNER) not found -- install wayland-dev (Alpine) or libwayland-bin (Debian)"; \
		exit 1; }
	$(WL_SCANNER) server-header $< $@

# wlroots reads these two enum headers with an explicit path; enum-header needs
# wayland-scanner 1.22, so older scanners fall back to the full header, which
# defines the same enum.
$(PROTO_DIR)/wayland-protocols/%-enum.h: protocols/%.xml | $(PROTO_DIR)/wayland-protocols
	@command -v $(WL_SCANNER) >/dev/null 2>&1 || { \
		echo "seraphim: $(WL_SCANNER) not found -- install wayland-dev (Alpine) or libwayland-bin (Debian)"; \
		exit 1; }
	$(WL_SCANNER) enum-header $< $@ 2>/dev/null || $(WL_SCANNER) client-header $< $@

build/angelscript/scriptstdstring.o: $(AS_STDSTRING) | build/angelscript
	$(CXX) -std=c++17 -O2 -I$(AS_DIR)/sdk/angelscript/include -c $< -o $@

build/angelscript/scriptarray.o: $(AS_ARRAY) | build/angelscript
	$(CXX) -std=c++17 -O2 -I$(AS_DIR)/sdk/angelscript/include -c $< -o $@

build/test_wm.o: tests/test_wm.cpp | build
	$(CXX) $(DEPFLAGS) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/%.o: tests/%.cpp | build

build/test_wm: build/test_wm.o $(CORE_OBJ) $(AS_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(AS_LIB) -lpthread -lm

test: build/test_wm
	./build/test_wm scripts/seraphim.as

build build/angelscript $(PROTO_DIR) $(PROTO_DIR)/wayland-protocols:
	mkdir -p $@

deps:
	@mkdir -p third_party
	@test -d "$(AS_DIR)" || git clone --depth 1 \
		https://github.com/codecat/angelscript-mirror "$(AS_DIR)"
	@$(MAKE) -C "$(AS_DIR)/sdk/angelscript/projects/gnuc"

install: seraphim seraphimctl
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m755 seraphim $(DESTDIR)$(PREFIX)/bin/seraphim
	install -m755 seraphimctl $(DESTDIR)$(PREFIX)/bin/seraphimctl
	mkdir -p $(DESTDIR)$(PREFIX)/share/seraphim
	install -m644 scripts/seraphim.as $(DESTDIR)$(PREFIX)/share/seraphim/seraphim.as
	mkdir -p $(DESTDIR)$(PREFIX)/share/wayland-sessions
	install -m644 seraphim.desktop $(DESTDIR)$(PREFIX)/share/wayland-sessions/seraphim.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/seraphim
	rm -f $(DESTDIR)$(PREFIX)/bin/seraphimctl
	rm -f $(DESTDIR)$(PREFIX)/share/seraphim/seraphim.as
	rm -f $(DESTDIR)$(PREFIX)/share/wayland-sessions/seraphim.desktop

clean:
	rm -rf build seraphim seraphimctl

# Header dependencies produced by -MMD above.
-include $(wildcard build/*.d) $(wildcard build/angelscript/*.d)

.PHONY: all test deps install uninstall clean
