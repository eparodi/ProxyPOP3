cmake_minimum_required(VERSION 3.8)
project(tpe_protos)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

AUX_SOURCE_DIRECTORY(src SOURCE_FILES)
add_executable(tpe_protos ${SOURCE_FILES})