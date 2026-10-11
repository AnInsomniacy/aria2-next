# The replacement core under engine/. It becomes the aria2-next executable once
# it reaches parity with the transfer validation suite.
file(GLOB_RECURSE ARIA2_ENGINE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cc")
list(REMOVE_ITEM ARIA2_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/engine/app/Main.cc")

# Everything but main(), shared by the executable and the unit tests.
add_library(aria2_engine STATIC ${ARIA2_ENGINE_SOURCES})
set_target_properties(aria2_engine PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)
target_include_directories(aria2_engine PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/engine"
  "${ARIA2_BOOST_ROOT}")
target_compile_definitions(aria2_engine PUBLIC
  ARIA2_VERSION="${PROJECT_VERSION}"
  BOOST_ASIO_HEADER_ONLY
  BOOST_ERROR_CODE_HEADER_ONLY
  BOOST_JSON_NO_LIB
  XML_STATIC)
target_link_libraries(aria2_engine PUBLIC
  spdlog::spdlog_header_only
  CURL::libcurl_static
  aria2::sqlite
  OpenSSL::SSL
  OpenSSL::Crypto
  aria2::gpac aria2::avformat aria2::avcodec aria2::swresample aria2::avutil
  aria2::expat
  aria2::zlib
  LibtorrentRasterbar::torrent-rasterbar
  Threads::Threads)
if(WIN32)
  target_compile_definitions(aria2_engine PUBLIC
    WINVER=${ARIA2_WINDOWS_VERSION}
    _WIN32_WINNT=${ARIA2_WINDOWS_VERSION}
    WIN32_LEAN_AND_MEAN
    NOMINMAX)
  target_link_libraries(aria2_engine PUBLIC
    ws2_32 mswsock iphlpapi crypt32 secur32 bcrypt advapi32 shell32 ole32 winmm)
endif()
if(APPLE)
  target_link_libraries(aria2_engine PUBLIC
    "-framework CoreFoundation" "-framework Security" "-framework Foundation")
endif()

add_executable(aria2-next-engine "${CMAKE_CURRENT_SOURCE_DIR}/engine/app/Main.cc")
set_target_properties(aria2-next-engine PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)
target_link_libraries(aria2-next-engine PRIVATE aria2_engine)

if(BUILD_TESTING)
  file(GLOB ARIA2_ENGINE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/engine/*.cc")
  add_executable(aria2-next-engine-tests ${ARIA2_ENGINE_TEST_SOURCES})
  set_target_properties(aria2-next-engine-tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
  target_include_directories(aria2-next-engine-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest")
  target_link_libraries(aria2-next-engine-tests PRIVATE aria2_engine)
  add_test(NAME engine COMMAND aria2-next-engine-tests)
endif()
