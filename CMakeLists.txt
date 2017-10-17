cmake_minimum_required(VERSION 3.8)
project(tpe_protos)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES src/main.c src/main.h src/MIMEparser.c src/MIMEparser.h src/external_transformation.c src/external_transformation.h)
add_executable(tpe_protos ${SOURCE_FILES})