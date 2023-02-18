#!/bin/sh
PKGS="x11 xft freetype2"
c++ `pkg-config --cflags $PKGS` -Wall -Wextra -o menu menu.cpp `pkg-config --libs $PKGS`
