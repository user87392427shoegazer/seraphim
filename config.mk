VERSION   = 0.1.0
PREFIX   ?= /usr/local
# gnu make's builtin CXX is g++, which `?=` never overrides; use the system
# c++ (clang on FreeBSD) unless the user set CXX explicitly
ifeq ($(origin CXX),default)
CXX = c++
endif
PKG_CONFIG ?= pkg-config

WLR_PC ?= wlroots-0.20

DEPS_PKGS = wayland-server xkbcommon libinput libdrm pixman-1

ANGELSCRIPT_DIR ?= third_party/angelscript

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),FreeBSD)
# ports install headers and libs under /usr/local
CPPFLAGS += -I/usr/local/include
LDFLAGS  += -L/usr/local/lib
endif
