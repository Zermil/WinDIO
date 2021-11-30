@echo off
set CXX=g++

set FLAGS=-Wall -Wextra -pedantic -std=c++17
set FILES=main.cpp
set LINK=-lwinmm -lpthread

call %CXX% %FILES% %FLAGS% %LINK% -o windio-test
