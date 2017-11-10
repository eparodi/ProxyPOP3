include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

# If pkg-config is not installed on the system, then the
# CHECK_INSTALL_DIR variable must be set to the install
# location of Check.
# set(CHECK_INSTALL_DIR "path")

find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})


# TEST 1
set(TEST_SOURCES
  check_parser.c
)

add_executable(check_parser ${TEST_SOURCES})
target_link_libraries(check_parser parser ${CHECK_LIBRARIES})

# TEST 2
set(TEST_SOURCES
  check_parser_utils.c
)

add_executable(check_parser_utils ${TEST_SOURCES})
target_link_libraries(check_parser_utils parser ${CHECK_LIBRARIES})

# TEST 3
set(TEST_SOURCES
  check_mime_msg.c
)

add_executable(check_mime_msg ${TEST_SOURCES})
target_link_libraries(check_mime_msg parser ${CHECK_LIBRARIES})

