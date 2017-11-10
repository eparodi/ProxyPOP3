# hay que ver como incluir todo de una (ver como lo hace CLion)
set(LIB_SOURCES
  parser.c
  mime_chars.c
  mime_msg.c
  mime_type.c
  parser_utils.c
  pop3_multi.c
  stripmime.c
  MIMEtree.c
)

set(MAIN_SOURCES
  main.c
)

set(HEADERS 
  ${CONFIG_HEADER}
  parser.h
  mime_chars.h
  mime_msg.h
  mime_type.h
  parser_utils.h
  pop3_multi.h
  stripmime.h
  MIMEtree.h
)

add_library(parser STATIC ${LIB_SOURCES} ${HEADERS})

add_executable(stripmime ${HEADERS} ${MAIN_SOURCES})
target_link_libraries(stripmime parser)

install(TARGETS parser
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/parser.h DESTINATION include)

