cmake_minimum_required(VERSION 3.16)

project(
    Sentinel
    VERSION 1.0.0
    DESCRIPTION "Simple Linux system monitoring agent"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_subdirectory(agent)
