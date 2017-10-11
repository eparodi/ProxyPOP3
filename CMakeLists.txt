cmake_minimum_required(VERSION 3.8)
project(tpe_protos)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES src/main.c src/main.h src/selector.h src/pop3.c src/pop3.h src/selector.c src/buffer.c src/buffer.h src/netutils.c src/netutils.h src/stm.c src/stm.h)
add_executable(tpe_protos ${SOURCE_FILES})