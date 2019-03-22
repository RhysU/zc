CFLAGS := -std=c99 -Wall -Wextra -Werror -g -O0 # -O3

all: zc

zc: zc.c

.PHONY: clean
clean:
	rm -f zc

SHELL := /bin/bash -o pipefail
.PHONY: check check_create check_all
check: check_create check_all
check_create: zc
	rm -f db.create
	./zc -d db.create
	test -f db.create
	rm db.create
check_all: zc
	rm -f db.all
	./zc -d db.all -a B
	./zc -d db.all -a C
	./zc -d db.all -a A
	cmp <(./zc -d db.all) <(echo -ne "A\nB\nC\n")
	rm db.all
