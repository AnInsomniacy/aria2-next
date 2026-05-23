configure_file(cmake/config.h.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include(cmake/Sources.cmake)
include(cmake/TestSources.cmake)
