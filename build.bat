@echo off
clang main.c src/cisp.c src/map.c src/console.c -o cisp.exe -O0 -gfull -g3 -Wall -Wno-switch -Wno-microsoft-enum-forward-reference -Wno-unused-variable -Wno-unused-function
@echo on