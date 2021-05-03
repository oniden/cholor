CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-lOpenCL -lm
TARGET_BIN=chlr.out

build: chlr.c
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $^ $(LDFLAGS)