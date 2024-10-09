#!/bin/sh

set -xe

LIBS="x11 xft freetype2"
FLAGS="compile_flags.txt"

pkg-config --cflags $LIBS | tr -s ' ' '\n' > $FLAGS
cc -O3 `cat $FLAGS` -o bin/menu src/*.c `pkg-config --libs $LIBS`
