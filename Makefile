CFLAGS := -std=c99 -Wall -Werror -O3

all: zc

zc: zc.c

.PHONY: clean
clean:
	rm -f zc
