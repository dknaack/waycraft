LDFLAGS = -lm -ldl `pkg-config --libs x11 glx`
CFLAGS  = -g -std=c11 -pedantic -Wall -D_POSIX_C_SOURCE=200809L `pkg-config --cflags x11 glx`
CC      = cc

SRC = src/main.c

all: options waycraft

options:
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "CC      = $(CC)"
	@echo

waycraft: $(SRC)
	$(CC) $(CFLAGS) -o $@ src/main.c $(LDFLAGS)
