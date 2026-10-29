#!/bin/sh
# Pull the Alpine wlroots 0.20 development headers into /tmp/sysroot, so that
# src/main.cpp can be syntax-checked here without installing anything
# system-wide. This is a development aid only; nothing in the build uses it.
#
#   sh tools/fetch-headers.sh
#   g++ -fsyntax-only -I/tmp/sysroot/usr/include/wlroots-0.20 ...
set -u

BASE=https://dl-cdn.alpinelinux.org/alpine/edge
IDX=/tmp/apk/idx
ROOT=/tmp/sysroot

mkdir -p "$IDX/main" "$IDX/community" "$ROOT"

fetch_index() {
	repo=$1
	[ -f "$IDX/$repo/APKINDEX" ] && return 0
	curl -fsSL "$BASE/$repo/x86_64/APKINDEX.tar.gz" -o "/tmp/apk/$repo.tgz" || return 1
	tar xzf "/tmp/apk/$repo.tgz" -C "$IDX/$repo" APKINDEX || {
		echo "fail: cannot unpack the $repo index"
		return 1
	}
}

apk_url() {
	name=$1
	repo=$2
	# APKINDEX records start with C: then P:, so match the P: line wherever
	# it is in the stanza rather than assuming it comes first.
	ver=$(awk -v p="P:$name" 'BEGIN{RS="";FS="\n"} {
		found = 0
		for (i = 1; i <= NF; i++)
			if ($i == p) found = 1
		if (!found) next
		for (i = 1; i <= NF; i++)
			if ($i ~ /^V:/) { sub(/^V:/, "", $i); print $i; exit }
	}' "$IDX/$repo/APKINDEX")
	[ -n "$ver" ] || return 1
	echo "$BASE/$repo/x86_64/$name-$ver.apk"
}

get_pkg() {
	name=$1
	repo=$2
	url=$(apk_url "$name" "$repo") || { echo "skip: $name not in $repo"; return 0; }
	file=/tmp/apk/$(basename "$url")
	[ -f "$file" ] || curl -fsSL "$url" -o "$file" || { echo "fail: $name"; return 0; }
	dir=/tmp/apk/x/$name
	mkdir -p "$dir"
	if tar xzf "$file" -C "$dir" data.tar.gz 2>/dev/null; then
		tar xzf "$dir/data.tar.gz" -C "$ROOT"
	else
		# apk v3: the payload is the archive itself, no wrapper tarball
		tar xzf "$file" -C "$ROOT" --exclude='.SIGN*' --exclude='.PKGINFO' \
			|| { echo "fail: $name"; return 0; }
	fi
	echo "ok: $(basename "$url")"
}

fetch_index main
fetch_index community

get_pkg wlroots0.20-dev community
get_pkg wayland-dev main
get_pkg wayland-protocols main
get_pkg libxkbcommon-dev main
get_pkg pixman-dev main
get_pkg libdrm-dev main
get_pkg libinput-dev community
get_pkg libevdev-dev community
get_pkg mtdev-dev community
get_pkg elogind-dev community
get_pkg libdisplay-info-dev main
get_pkg hwdata main

echo
echo "headers in $ROOT"
