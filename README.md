[![Build Status](https://travis-ci.org/RhysU/zc.svg?branch=master)](https://travis-ci.org/RhysU/zc)

# zc

## What is zc?

`zc` is a utility for tracking which directories you have recently
visited.  With a little shell integration effort, it permits "jumping
around".  The utility was written after some experimentation with
[rupa/z](https://github.com/rupa/z).

Relative to rupa/z's [z.sh](https://github.com/rupa/z/blob/master/z.sh), zc has
the following advantages:

 1. zc isolates maintaining the recently-visited database from shell details.
 2. zc uses flocks to avoid clobbering the database in, e.g., GNU Screen.
 3. zc has the overhead like launching one single process.
 4. zc has unit tests and some minimal performance testing.

Relative to rupa/z, `zc` has the following disadvantages:

 1. zc must be compiled *and* you have to read [my lousy C](zc.c).
 2. The mechanism for cleaning up subsequently removed directories might
    be unfriendly to network filesystems.
 3. Regex search is not implemented.
 4. Bash integration (found below) requires
    [bash-preexec](https://github.com/rcaloras/bash-preexec).
 5. Bash integration presently lacks tab completion.
 6. Zero adoption by anyone but me.

## How do I use it?

After creating some directories and visiting them once, attempting to jump
to a directory containing 'q' is ambiguous.  The ambiguity is broken by
asking for the most recently visited match (-t):
```
$ mkdir -p foo/bar/baz/qux/quux
$ cd foo/bar/baz/qux/quux/
$ cd ..
$ cd ..
$ cd ..
$ cd ..
$ cd ..
$ z q
zc/foo/bar/baz/qux
zc/foo/bar/baz/qux/quux
$ z -t q
~/Build/zc/foo/bar/baz/qux ~/Build/zc
```
The above ambiguity could also be settled by providing, e.g., "quu".  Instead
of most recent time (-t), one could request highest rank (-r) according to
number of visits or by "[frecency](https://en.wikipedia.org/wiki/Frecency)"
(-f).  Multiple path elements could be used, e.g. `ux ux`, and all of them
must match in order.

Notice `z` appeared above instead of `zc`.  That's a consequence of
the following installation.

## How do I install it?

Installing `zc` involves something like the following in your `.bashrc`.

```bash
# Aliases ease using https://github.com/RhysU/zc against a particular database
if test -x "${HOME}/bin/zc"; then
    z_raw() { "$HOME/bin/zc" -d "$HOME/.zc"       "$@"; }
    z_add() { "$HOME/bin/zc" -d "$HOME/.zc" -a -- "$@"; }
else
    z_raw() { echo "RhysU/zc is not installed" 1>&2; false; }
    z_add() { :; }
fi

# Run http://github.com/RhysU/zc in some mode against fixed database.
# Whenever it succeeds and only one line is output, attempt to pushd there.
# Takes advantage of knowledge that zc has a 2-line help message.
# Nicer would be providing a more-useful -h message, but so it goes.
z() {
    local zout newline='
'
    if zout=$(z_raw "$@"); then
        case $zout in
          *"$newline"*) echo "$zout"          ;;
                     *) builtin pushd "$zout" ;;
        esac
    fi
}

# On every command, record the working directory using z_add.
# Versus https://github.com/rupa/z, updating per-command nicer for GNU Screen.
# Also, the updates are lighter weight on account of backing zc implementation.
precmd_z() {
    # Only register non-HOME directories as HOME is just a 'cd' away.
    if [ "$PWD" != "$HOME" ]; then
        z_add "$PWD"
    fi
}

# Prompts use precmd/preexec hook per https://github.com/rcaloras/bash-preexec.
if [ -f "${HOME}/.preexec.bash" ]; then
    . "${HOME}/.preexec.bash"
    precmd_functions+=(precmd_z)
fi
```

Golfing is most welcome, especially golfing that addresses disadvantages.  Not
breaking Bash login on systems without either
[bash-preexec](https://github.com/rcaloras/bash-preexec) or `zc` available is
desired.
