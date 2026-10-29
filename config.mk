# seraphim build settings, included by the Makefile.

VERSION   = 0.1.0
PREFIX   ?= /usr/local
CXX      ?= c++
PKG_CONFIG ?= pkg-config

# The wlroots package to build the host against. wlroots-0.20 is the packaged
# name on Alpine (wlroots0.20-dev) and Debian testing (libwlroots-0.20-dev).
WLR_PC ?= wlroots-0.20

# Everything the host links besides wlroots.
DEPS_PKGS = wayland-server xkbcommon libinput libdrm pixman-1

# Where the AngelScript SDK lives. `make deps` clones and builds it here.
ANGELSCRIPT_DIR ?= third_party/angelscript
