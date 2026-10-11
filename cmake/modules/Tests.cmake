file(GLOB ARIA2_TEST_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cc")
add_executable(aria2_tests ${ARIA2_TEST_SOURCES})
target_include_directories(aria2_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest")
target_link_libraries(aria2_tests PRIVATE aria2_core)
add_test(NAME aria2_tests COMMAND aria2_tests)
