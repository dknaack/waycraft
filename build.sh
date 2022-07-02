#!/bin/sh

set -e

waycraft_libs() {
	pkg-config --cflags --libs xcb xcb-xfixes xcb-composite \
		wayland-server wayland-client wayland-egl xkbcommon-x11 egl gl
}

wayland_scanner() {
	[ -f "$3" ] || wayland-scanner "$1" "/usr/share/wayland-protocols/$2" "$3"
}

cflags() {
	echo -g -std=c11 -pedantic -I. -fPIC \
		-Wall -Werror=implicit-function-declaration -Werror=incompatible-pointer-types \
		-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=600 -DENABLE_XWAYLAND=1
}

[ ! -d "build" ] && mkdir build
[ ! -f "build/stb_image.o" ] && cc $(cflags) -c waycraft/stb_image.c -o build/stb_image.o

echo "Generating headers and code..."
wayland_scanner private-code  stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
wayland_scanner server-header stable/xdg-shell/xdg-shell.xml xdg-shell-server-protocol.h
wayland_scanner client-header stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h

echo "Building waycraft..."
cc $(cflags) -o build/waycraft waycraft/main.c $(waycraft_libs) -lm &
cc $(cflags) -shared -o build/libgame.so build/stb_image.o waycraft/game.c -lm &
wait
