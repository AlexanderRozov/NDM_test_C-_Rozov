CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700
LDFLAGS =

SRCS = src/main.c src/tty.c src/dict.c src/match.c src/modem.c src/selftest.c
OBJS = $(SRCS:src/%.c=build/%.o)

.PHONY: all clean test

all: at-server

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

at-server: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

test: at-server
	./at-server --self-test -f data/responses.csv

clean:
	rm -rf build at-server
