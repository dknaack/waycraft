#!/bin/sh

set -e

waycraft_libs() {
	pkg-config --cflags --libs xcb xcb-xfixes xcb-composite \
		wayland-server xkbcommon-x11 egl gl
}

cflags() {
	echo -g -std=c11 -pedantic \
		-Wall -Wno-unused-function -Werror=implicit-function-declaration -fPIC \
		-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=600 -I. -DENABLE_XWAYLAND=1
}

[ ! -d "build" ] && mkdir build

echo "Building waycraft..."
cc $(cflags) -o build/waycraft waycraft/main.c $(waycraft_libs) -lm &
cc $(cflags) -shared -o build/libgame.so waycraft/game.c -lm &
wait
