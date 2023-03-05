#!/bin/sh -e
c++ -Wall -Wextra -o menu menu.cpp `pkg-config --cflags --libs x11 xft freetype2`
