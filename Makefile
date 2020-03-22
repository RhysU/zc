all: zc

VERSION := $(shell git describe --always --tags --dirty)
CFLAGS := -std=c99 -Wall -Wextra -Werror -O3 -static -DVERSION=$(VERSION)
zc: zc.c

.PHONY: clean
clean:
	rm -f zc

.PHONY: install
install: zc
	/usr/bin/install --verbose $< ${HOME}/bin/$<

# Simple unit testing employs some Bashisms combined with GNU Make
SHELL := /bin/bash -o pipefail

# Help/version messages must be emitted and a database is required
CHECKS = check_help
check_help:
	./zc -h | awk 'END {exit(NR!=2)}'  # Zero exit with exactly 2 lines
	./zc -v | awk 'END {exit(NR!=1)}'  # Zero exit with exactly 1 line
	(./zc && false) || true            # Non-zero exit

# A non-existent database must be created with zero length
CHECKS += check_create
check_create:
	rm -f db.create
	./zc -d db.create
	test -f db.create
	test ! -s db.create
	rm db.create

# All unique names added must be emitted in lexicographic order
CHECKS += check_all
check_all:
	rm -f db.all
	./zc -d db.all -ka A
	./zc -d db.all -ka A
	./zc -d db.all -ka B
	./zc -d db.all -ka C
	./zc -d db.all -ka dd
	./zc -d db.all -ka C
	./zc -d db.all -ka A
	cmp <(./zc -d db.all) <(echo -ne "A\nB\nC\ndd\n")
	rm db.all

# Non-existent names must be removed from the database
CHECKS += check_keep
check_keep:
	rm -f db.keep
	./zc -d db.keep -a A
	cmp <(./zc -d db.keep) <(echo -ne)        # A not present
	touch A B
	./zc -d db.keep -a A
	cmp <(./zc -d db.keep) <(echo -ne "A\n")  # A now present
	rm A
	./zc -d db.keep -a B
	cmp <(./zc -d db.keep) <(echo -ne "B\n")  # A disappeared
	rm B
	rm db.keep

# All partial matches of any substring must be emitted
CHECKS += check_match
check_match:
	rm -f db.match
	./zc -d db.match -ka foo
	./zc -d db.match -ka Foobar
	./zc -d db.match -ka barBaz
	./zc -d db.match -ka qux
	cmp <(./zc -d db.match '' '') <(echo -ne "Foobar\nbarBaz\nfoo\nqux\n")
	cmp <(./zc -d db.match bar) <(echo -ne "Foobar\nbarBaz\n")
	cmp <(./zc -d db.match Foo) <(echo -ne "Foobar\n")
	cmp <(./zc -d db.match 'ba' 'z') <(echo -ne "barBaz\n")
	rm db.match

# Only print the output with the highest rank (and therefore frecency)
CHECKS += check_rank
check_rank:
	rm -f db.rank
	if ./zc -d db.rank -r; then false; else true; fi  # Nothing found is error
	if ./zc -d db.rank -f; then false; else true; fi  # Nothing found is error
	./zc -d db.rank -ka foo
	cmp <(./zc -d db.rank -r oo) <(echo -ne "foo\n")  # Just one
	cmp <(./zc -d db.rank -f oo) <(echo -ne "foo\n")  # Frequency like rank
	./zc -d db.rank -ka Foo
	cmp <(./zc -d db.rank -r oo) <(echo -ne "Foo\n")  # Time breaks ties
	cmp <(./zc -d db.rank -f oo) <(echo -ne "Foo\n")  # Frequency like rank
	./zc -d db.rank -ka foo
	cmp <(./zc -d db.rank -r oo) <(echo -ne "foo\n")
	cmp <(./zc -d db.rank -f oo) <(echo -ne "foo\n")  # Frequency like rank
	rm db.rank

# Only print the output that was most recently added
CHECKS += check_time
check_time:
	rm -f db.time
	if ./zc -d db.time -t; then false; else true; fi  # Nothing found is error
	./zc -d db.time -ka foo
	cmp <(./zc -d db.time -r oo) <(echo -ne "foo\n")  # Just one
	./zc -d db.time -ka foo
	./zc -d db.time -ka Foo
	cmp <(./zc -d db.time -t oo) <(echo -ne "Foo\n")  # Most recent
	./zc -d db.time -ka baz bar                       # Add multiple
	cmp <(./zc -d db.time -t ba) <(echo -ne "bar\n")  # Name breaks ties
	rm db.time

# Run all registered checks on 'make check' target
check: ${CHECKS}
.PHONY: check ${CHECKS}
${CHECKS}: zc

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
	time xargs --arg-file=in.stress -n 1 ./zc -d db.stress -ka
	time xargs --arg-file=in.stress -n 1 ./zc -d db.stress > /dev/null
	rm db.stress
