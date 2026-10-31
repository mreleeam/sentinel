cmake_minimum_required(VERSION 3.16)
project(SentinelAgent LANGUAGES C)

add_executable(sentinel-agent
    src/main.c
    src/config.c
    src/logger.c
    src/collector_manager.c
    src/cpu.c
    src/memory.c
    src/disk.c
    src/network.c
    src/process.c
    src/swap.c
    src/state.c
)

target_include_directories(sentinel-agent
    PRIVATE
        include
)

target_compile_options(sentinel-agent PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

install(
    TARGETS sentinel-agent
    RUNTIME DESTINATION bin
)
