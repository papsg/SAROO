CC = gcc
CFLAGS ?= -Wall -g -O3
LDFLAGS ?= -s

.PHONY: all cdgfix
all: cdgfix
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o cdgplay cdgplay.c gdifb.c -lgdi32

cdgfix:
	$(CC) $(CPPFLAGS) -D_FILE_OFFSET_BITS=64 $(CFLAGS) $(LDFLAGS) -o cdgfix cdgfix.c
