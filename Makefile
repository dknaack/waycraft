WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)
LIBS=\
	 $(shell pkg-config --cflags --libs wlroots) \
	 $(shell pkg-config --cflags --libs wayland-server) \
	 $(shell pkg-config --cflags --libs xkbcommon) \
	 $(shell pkg-config --cflags --libs egl gl glew)

LDFLAGS = -lm -ldl $(LIBS)
CFLAGS  = -g -std=c11 -pedantic -Wall -D_POSIX_C_SOURCE=200809L \
		  -DWLR_USE_UNSTABLE -Iinclude
CC      = cc

all: options waycraft

options:
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "CC      = $(CC)"
	@echo

waycraft: src/stb_image.o src/xdg-shell-protocol.c
	$(CC) $(CFLAGS) -o $@ src/main.c src/stb_image.o $(LDFLAGS)

src/stb_image.o:
	$(CC) $(CFLAGS) -DSTB_IMAGE_IMPLEMENTATION -c -o $@ src/stb_image.c

include/xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

src/xdg-shell-protocol.c: include/xdg-shell-protocol.h
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

.PHONY: waycraft
