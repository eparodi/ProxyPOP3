cmake_minimum_required(VERSION 3.5)
project(tpe_protos)

set(CMAKE_VERBOSE_MAKEFILE ON)

# set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread --std=c99 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -D_POSIX_C_SOURCE=200112L -fsanitize=address")
add_compile_options("-lsctp")
link_libraries("-lsctp")

AUX_SOURCE_DIRECTORY(pop3filter/src SOURCE_FILES)
add_executable(pop3filter ${SOURCE_FILES})

AUX_SOURCE_DIRECTORY(pop3ctl/src POP3CTL_SOURCE_FILES)
add_executable(pop3ctl ${POP3CTL_SOURCE_FILES})

AUX_SOURCE_DIRECTORY(stripmime/src STRIPMIME_SOURCE_FILES)
add_executable(stripmime ${STRIPMIME_SOURCE_FILES})