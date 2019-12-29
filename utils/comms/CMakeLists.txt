cmake_minimum_required(VERSION 2.8.8)
cmake_policy(SET CMP0048 NEW)
project(le_das_utils VERSION 2.0.0)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_definitions( -D_POSIX_SOURCE=1 -D_XOPEN_SOURCE=500 )

add_subdirectory(tm_relay)
