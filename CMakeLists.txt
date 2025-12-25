cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_COMPILER clang)

set(CMAKE_GENERATOR Ninja)

project(audio-analyzer)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)

add_executable(audio-analyzer src/main.c)

target_compile_options(audio-analyzer PRIVATE
        -pthread
        -Wall -Werror
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-unused-label

        $<$<CONFIG:Debug>:
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -mno-omit-leaf-frame-pointer
        -fno-inline
        -g
        -O0
        >

        $<$<CONFIG:Release>:
        -O3
        >
)

target_link_options(audio-analyzer PRIVATE
        -lm

        $<$<CONFIG:Debug>:
        -fsanitize=address,undefined
        >
)

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    find_package(yaml-cpp REQUIRED)
    target_link_libraries(audio-analyzer PUBLIC yaml-cpp)
    include(FetchContent)
    option(TRACY_ENABLE "" ON)
    option(TRACY_ON_DEMAND "" ON)
    FetchContent_Declare(
            tracy
            GIT_REPOSITORY https://github.com/wolfpld/tracy.git
            GIT_TAG v0.13.0
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(tracy)
    target_link_libraries(audio-analyzer PUBLIC TracyClient)
endif ()

