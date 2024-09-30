@echo off
clang src/test.c src/map.c src/console.c -o _test.exe -O0 -gfull -g3 -Wall -Wno-switch -Wno-microsoft-enum-forward-reference -Wno-unused-variable -Wno-unused-function
_test.exe
@echo on