include config.mk

CXXFLAGS  += -std=c++17 -O2 $(if $(DEBUG),-g) -Wall -Wextra -Wno-unused-parameter

# freebsd hides SOCK_CLOEXEC & co when _POSIX_C_SOURCE is set explicitly
ifneq ($(UNAME_S),FreeBSD)
CXXFLAGS  += -D_POSIX_C_SOURCE=200809L
endif

CXXFLAGS  += -fno-omit-frame-pointer -no-pie -ffunction-sections -fdata-sections

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

CPPFLAGS += -DSERAPHIM_VERSION=\"$(VERSION)\" -DWLR_PC=\"$(WLR_PC)\"

CORE_SRC = src/canvas.cpp src/borders.cpp src/world.cpp src/script.cpp src/ipc.cpp
HOST_SRC = src/main.cpp

CORE_OBJ = $(patsubst src/%.cpp,build/%.o,$(CORE_SRC))
HOST_OBJ = $(patsubst src/%.cpp,build/%.o,$(HOST_SRC))
AS_OBJ   = build/angelscript/scriptstdstring.o build/angelscript/scriptarray.o

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
		$(DEPS_LIBS) $(WLR_LIBS) -lpthread -lm $(LDFLAGS) -Wl,--gc-sections

seraphimctl: tools/seraphimctl.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

build/%.o: src/%.cpp | build
	$(CXX) $(DEPFLAGS) $(CPPFLAGS) $(AS_CPPFLAGS) $(CXXFLAGS) $(WLR_CFLAGS) $(DEPS_CFLAGS) -c $< -o $@

build/main.o: src/main.cpp $(PROTO_HDR) $(PROTO_ENUM) | build
	$(CXX) $(DEPFLAGS) $(CPPFLAGS) $(AS_CPPFLAGS) $(PROTO_CFLAGS) $(CXXFLAGS) \
		$(WLR_CFLAGS) $(DEPS_CFLAGS) -c $< -o $@

$(PROTO_DIR)/%-protocol.h: protocols/%.xml | $(PROTO_DIR)
	@command -v $(WL_SCANNER) >/dev/null 2>&1 || { \
		echo "seraphim: $(WL_SCANNER) not found -- install wayland-dev (Alpine), libwayland-bin (Debian) or wayland (FreeBSD)"; \
		exit 1; }
	$(WL_SCANNER) server-header $< $@

$(PROTO_DIR)/wayland-protocols/%-enum.h: protocols/%.xml | $(PROTO_DIR)/wayland-protocols
	@command -v $(WL_SCANNER) >/dev/null 2>&1 || { \
		echo "seraphim: $(WL_SCANNER) not found -- install wayland-dev (Alpine), libwayland-bin (Debian) or wayland (FreeBSD)"; \
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
	@$(MAKE) -C "$(AS_DIR)/sdk/angelscript/projects/gnuc" CXX="$(CXX)"

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

-include $(wildcard build/*.d) $(wildcard build/angelscript/*.d)

.PHONY: all test deps install uninstall clean
