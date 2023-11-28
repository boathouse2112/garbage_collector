CC = clang
CFLAGS=-Wall -Wextra -Werror -std=c99 -g

source = gc.c
objects = ${patsubst %.c,%.o,${source}}

.PHONY: all
all: gc

gc: $(objects)
	$(CC) $(CFLAGS) $(objects) -o gc

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(objects) gc
