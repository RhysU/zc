CFLAGS := -std=c99 -Wall -Wextra -Werror -g -O0 # -O3

all: zc

zc: zc.c

.PHONY: clean
clean:
	rm -f zc

# Simple unit testing employs some Bashisms combined with GNU Make
SHELL  := /bin/bash -o pipefail
CHECKS := check_create check_all check_match check_rank
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

# All partial matches of any substring must be emitted
check_match:
	rm -f db.match
	./zc -d db.match -a foo
	./zc -d db.match -a Foobar
	./zc -d db.match -a barBaz
	./zc -d db.match -a qux
	cmp <(./zc -d db.match '' '') <(echo -ne "Foobar\nbarBaz\nfoo\nqux\n")
	cmp <(./zc -d db.match bar) <(echo -ne "Foobar\nbarBaz\n")
	cmp <(./zc -d db.match Foo) <(echo -ne "Foobar\n")
	cmp <(./zc -d db.match 'ba' 'z') <(echo -ne "barBaz\n")
	rm db.match

# Only the output with the highest rank must be printed
check_rank:
	rm -f db.rank
	./zc -d db.rank -a foo
	./zc -d db.rank -a Foo
	./zc -d db.rank -a foo
	cmp <(./zc -d db.rank -r oo) <(echo -ne "foo\n")  # Rank
	cmp <(./zc -d db.rank -f oo) <(echo -ne "foo\n")  # Frecency
	rm db.rank
