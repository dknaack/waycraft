WAYLAND_PROTOCOLS = $(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER   = $(shell pkg-config --variable=wayland_scanner wayland-scanner)

WAYCRAFT_LIBS = \
	 $(shell pkg-config --cflags --libs xcb xcb-xfixes) \
	 $(shell pkg-config --cflags --libs wayland-server) \
	 $(shell pkg-config --cflags --libs xkbcommon-x11) \
	 $(shell pkg-config --cflags --libs egl gl) \
	 -lm

WCDB_LIBS = \
	$(shell pkg-config --cflags --libs wayland-client)

CFLAGS  = -g -std=c11 -pedantic -Wall -Wno-unused-function -D_POSIX_C_SOURCE=200809L -I.
CC      = cc

all: options build/waycraft build/wcdb

options:
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "CC      = $(CC)"
	@echo

build/waycraft: waycraft/stb_image.o waycraft/xdg-shell-protocol.c
	@mkdir -p build/
	@printf "CCLD\t$@\n"
	@$(CC) $(CFLAGS) -o $@ waycraft/main.c waycraft/stb_image.o $(WAYCRAFT_LIBS)

build/wcdb: wcdb/main.c wcdb/xdg-shell-protocol.c
	@printf "CCLD\t$@\n"
	@$(CC) $(CFLAGS) -o $@ wcdb/main.c $(WCDB_LIBS)

waycraft/stb_image.o:
	@printf "CC\t$@\n"
	@$(CC) $(CFLAGS) -DSTB_IMAGE_IMPLEMENTATION -c -o $@ waycraft/stb_image.c

waycraft/xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

waycraft/xdg-shell-protocol.c: waycraft/xdg-shell-protocol.h
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

wcdb/xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) client-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

wcdb/xdg-shell-protocol.c: wcdb/xdg-shell-protocol.h
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

.PHONY: build/waycraft build/wcdb
