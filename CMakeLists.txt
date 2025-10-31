cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_COMPILER clang)

set(CMAKE_GENERATOR Ninja)

project(audio-analyzer)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)

# -fsanitize=thread,undefined
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} \
    -pthread \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -mno-omit-leaf-frame-pointer \
    -fno-inline \
    -g -O0 \
    -Wall -Werror \
    -Wno-unused-function \
    -Wno-unused-variable \
    -Wno-unused-label")

add_executable(audio-analyzer src/main.c)
