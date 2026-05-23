configure_file(cmake/config.h.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(
  cmake/libaria2.pc.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/src/libaria2.pc
  @ONLY)

include(cmake/Sources.cmake)
include(cmake/TestSources.cmake)
