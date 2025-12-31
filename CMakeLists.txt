cmake_minimum_required(VERSION 4.0)

project(audio-analyzer)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)

set(CMAKE_CXX_STANDARD 17)

add_executable(audio-analyzer src/main.c)

if (MSVC)
    target_compile_options(audio-analyzer PRIVATE
            /WX

            /wd4100 # unused parameter
            /wd4101 # unused local variable
            /wd4102 # unreferenced label
            /wd4189 # local variable is initialized but not referenced

            /std:c11
            /experimental:c11atomics

            $<$<CONFIG:Debug>:
            /Od
            /fsanitize=address
            /RTCcsu
            >

            $<$<CONFIG:Release>:
            /O2
            >
    )
else ()
    target_compile_options(audio-analyzer PRIVATE
            -Wall -Werror

            -Wno-unused-function
            -Wno-unused-variable
            -Wno-unused-label

            -fno-fast-math

            $<$<CONFIG:Debug>:
            -fno-omit-frame-pointer
            -mno-omit-leaf-frame-pointer
            -fno-inline
            -g
            -O0

            $<$<NOT:$<BOOL:${MINGW}>>:-fsanitize=address,undefined>
            >

            $<$<CONFIG:Release>:
            -O3
            >
    )

    target_link_options(audio-analyzer PRIVATE
            $<$<PLATFORM_ID:Linux>:-lm>

            $<$<CONFIG:Debug>:
            $<$<NOT:$<BOOL:${MINGW}>>:-fsanitize=address,undefined>
            >
    )
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions(USE_TRACY)

    include(FetchContent)
    FetchContent_Declare(
            yaml-cpp
            GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(yaml-cpp)
    target_link_libraries(audio-analyzer PUBLIC yaml-cpp)

    include(FetchContent)
    option(TRACY_ENABLE "" ON)
    option(TRACY_ON_DEMAND "" ON)
    FetchContent_Declare(
            tracy
            GIT_REPOSITORY https://github.com/wolfpld/tracy.git
            GIT_TAG v0.13.1
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(tracy)
    target_link_libraries(audio-analyzer PUBLIC TracyClient)
endif ()
