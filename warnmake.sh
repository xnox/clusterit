#!/bin/sh -x
make CFLAGS='"-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wno-uninitialized -Wreturn-type -Wcast-qual -Wpointer-arith -Wwrite-strings -Wswitch -Wshadow -Wnetbsd-format-audit -Wno-format-extra-args -Werror -O2"'

