#!/bin/sh
PKGS="x11 xft freetype2"
c++ `pkg-config --cflags $PKGS` -Wall -Wextra -std=c++17 -pedantic -o bin/menu src/main.cpp -lX11 `pkg-config --libs $PKGS`
