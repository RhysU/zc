CFLAGS := -std=c99 -Wall -Wextra -Werror -g -O0 # -O3

all: zc

zc: zc.c

.PHONY: clean
clean:
	rm -f zc

# Simple unit testing employs some Bashisms combined with GNU Make
SHELL  := /bin/bash -o pipefail
CHECKS := check_create check_all check_match
check: ${CHECKS}
.PHONY: check ${CHECKS}
${CHECKS}: zc

# A non-existent database must be created with zero length
check_create:
	rm -f db.create
	./zc -d db.create
	test -f db.create
	test ! -s db.create
	rm db.create

# All unique names added must be emitted in lexicographic order
check_all:
	rm -f db.all
	./zc -d db.all -a A
	./zc -d db.all -a A
	./zc -d db.all -a B
	./zc -d db.all -a C
	./zc -d db.all -a dd
	./zc -d db.all -a C
	./zc -d db.all -a A
	cmp <(./zc -d db.all) <(echo -ne "A\nB\nC\ndd\n")
	rm db.all
