cmake_minimum_required(VERSION 3.16)
project(smallShell)

set(CMAKE_CXX_STANDARD 14)

add_executable(smallShell
        makefile
        p2testscript
        README.md
        smallsh.c)
