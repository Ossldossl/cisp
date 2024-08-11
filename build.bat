@echo off
clang main.c src/cisp.c src/console.c -fsanitize=address -o cisp.exe -O0 -gfull -g3 -Wall -Wno-switch -Wno-microsoft-enum-forward-reference -Wno-unused-variable -Wno-unused-function
@echo on