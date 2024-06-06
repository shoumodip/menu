#!/bin/sh

LIBS="x11 xft freetype2"
FLAGS="compile_flags.txt"

if [ ! -f $FLAGS ]; then
  pkg-config --cflags $LIBS | tr -s ' ' '\n' > $FLAGS
fi

cc `cat $FLAGS` -o menu main.c `pkg-config --libs $LIBS`
