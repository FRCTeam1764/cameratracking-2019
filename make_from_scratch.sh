#!/bin/bash
rm -rf CMakeFiles CMakeCache.txt Makefile
CXX=g++-8 CC=gcc-8 cmake . 
make -j5

