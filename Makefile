CFLAGS := -std=c99 -Wall -Wextra -Werror -g -O0 # -O3

all: zc

zc: zc.c

.PHONY: clean
clean:
	rm -f zc
