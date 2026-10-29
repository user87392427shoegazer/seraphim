VERSION   = 0.1.0
PREFIX   ?= /usr/local
CXX      ?= c++
PKG_CONFIG ?= pkg-config

WLR_PC ?= wlroots-0.20

DEPS_PKGS = wayland-server xkbcommon libinput libdrm pixman-1

ANGELSCRIPT_DIR ?= third_party/angelscript
