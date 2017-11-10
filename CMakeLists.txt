cmake_minimum_required(VERSION 3.8)
project(tpe_protos)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
add_compile_options("-lsctp")
link_libraries("-lsctp")

AUX_SOURCE_DIRECTORY(pop3filter/src SOURCE_FILES)
add_executable(tpe_protos ${SOURCE_FILES})