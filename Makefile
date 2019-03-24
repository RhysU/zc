CFLAGS := -std=c99 -Wall -Wextra -Werror -O3 -static

all: zc

zc: zc.c

.PHONY: clean
clean:
	rm -f zc

# Simple unit testing employs some Bashisms combined with GNU Make
SHELL  := /bin/bash -o pipefail
CHECKS := check_help check_create check_all check_match check_rank check_time
check: ${CHECKS}
.PHONY: check ${CHECKS}
${CHECKS}: zc

# Help messages must be emitted and a database is required
check_help:
	./zc -h | awk 'END {exit(NR!=2)}'  # Zero exit with exactly 2 lines
	(./zc && false) || true            # Non-zero exit

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

# Only print the output with the highest rank (and therefore frecency)
check_rank:
	rm -f db.rank
	./zc -d db.rank -a foo
	cmp <(./zc -d db.rank -r oo) <(echo -ne "foo\n")  # Just one
	cmp <(./zc -d db.rank -f oo) <(echo -ne "foo\n")  # Frequency like rank
	./zc -d db.rank -a Foo
	cmp <(./zc -d db.rank -r oo) <(echo -ne "Foo\n")  # Time breaks ties
	cmp <(./zc -d db.rank -f oo) <(echo -ne "Foo\n")  # Frequency like rank
	./zc -d db.rank -a foo
	cmp <(./zc -d db.rank -r oo) <(echo -ne "foo\n")
	cmp <(./zc -d db.rank -f oo) <(echo -ne "foo\n")  # Frequency like rank
	rm db.rank

# Only print the output that was most recently added
check_time:
	rm -f db.time
	./zc -d db.time -a foo
	cmp <(./zc -d db.time -r oo) <(echo -ne "foo\n")  # Just one
	./zc -d db.time -a foo
	./zc -d db.time -a Foo
	cmp <(./zc -d db.time -t oo) <(echo -ne "Foo\n")  # Most recent
	./zc -d db.time -a baz bar                        # Add multiple
	cmp <(./zc -d db.time -t ba) <(echo -ne "bar\n")  # Name breaks ties
	rm db.time

# A performance sanity check over half-sane workloads
.PHONY: stress
in.stress: Makefile
	rm -f $@
	head -c 131072 /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 23 > $@.tmp
	cat <(head -n  10 $@.tmp) \
	    <(head -n  15 $@.tmp) \
	    <(head -n  25 $@.tmp) \
	    <(head -n  35 $@.tmp) \
	    <(head -n  50 $@.tmp) \
	    <(head -n  75 $@.tmp) \
	    <(head -n 100 $@.tmp) \
	    <(head -n 200 $@.tmp) \
	    $@.tmp                | shuf > $@
	rm -f $@.tmp
stress: zc in.stress
	rm -f db.stress
	wc in.stress
	time xargs --arg-file=in.stress -n 1 ./zc -d db.stress -a
	time xargs --arg-file=in.stress -n 1 ./zc -d db.stress > /dev/null
	rm db.stress
