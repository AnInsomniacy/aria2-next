set(ARIA2_CORE_SOURCES ${ARIA2_SOURCES_BASE})
if(APPLE)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_APPLE_STATE_PATH})
elseif(WIN32)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_WINDOWS_STATE_PATH})
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_MINGW_BUILD})
else()
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_POSIX_STATE_PATH})
endif()
if(ENABLE_WEBSOCKET)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_ENABLE_WEBSOCKET})
else()
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_NOT_ENABLE_WEBSOCKET})
endif()
if(HAVE_LIBEXPAT)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_XML})
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_HAVE_LIBEXPAT})
endif()
if(ENABLE_XML_RPC)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_ENABLE_XML_RPC})
endif()
if(HAVE_SOME_FALLOCATE)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_HAVE_SOME_FALLOCATE})
endif()
if(HAVE_EPOLL)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_HAVE_EPOLL})
endif()
if(ENABLE_SSL)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_ENABLE_SSL})
endif()
list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_OPENSSL_CRYPTO})
if(HAVE_OPENSSL)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_HAVE_OPENSSL_TLS})
endif()
if(HAVE_ZLIB)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_HAVE_ZLIB})
endif()
if(HAVE_SQLITE3)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_HAVE_SQLITE3})
endif()
list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_ARC4})
if(ENABLE_BITTORRENT)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_ENABLE_BITTORRENT})
endif()
if(ENABLE_METALINK)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_ENABLE_METALINK})
endif()
if(NOT HAVE_GAI_STRERROR)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_NOT_HAVE_GAI_STRERROR})
endif()
if(NOT HAVE_GETTIMEOFDAY)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_NOT_HAVE_GETTIMEOFDAY})
endif()
if(NOT HAVE_LOCALTIME_R)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_NOT_HAVE_LOCALTIME_R})
endif()
if(NOT HAVE_STRPTIME)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_NOT_HAVE_STRPTIME})
endif()
if(NOT HAVE_TIMEGM)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_NOT_HAVE_TIMEGM})
endif()
if(NOT HAVE_DAEMON)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_NOT_HAVE_DAEMON})
endif()
if(HAVE_POLL)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_HAVE_POLL})
endif()
if(HAVE_KQUEUE)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_HAVE_KQUEUE})
endif()
if(ENABLE_LIBARIA2)
  list(APPEND ARIA2_CORE_SOURCES ${ARIA2_SOURCES_ENABLE_LIBARIA2})
endif()

list(REMOVE_DUPLICATES ARIA2_CORE_SOURCES)

find_package(Threads REQUIRED)
add_library(aria2_spdlog INTERFACE)
add_library(spdlog::spdlog_header_only ALIAS aria2_spdlog)
target_include_directories(aria2_spdlog INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/include)
target_compile_definitions(aria2_spdlog INTERFACE
  SPDLOG_DISABLE_DEFAULT_LOGGER
  SPDLOG_NO_THREAD_ID
  SPDLOG_PREVENT_CHILD_FD)
target_link_libraries(aria2_spdlog INTERFACE Threads::Threads)
if(WIN32)
  target_compile_definitions(aria2_spdlog INTERFACE
    SPDLOG_UTF8_TO_WCHAR_CONSOLE
    SPDLOG_WCHAR_FILENAMES)
endif()

if(ENABLE_WEBSOCKET)
  add_library(wslay STATIC
    third_party/wslay/lib/wslay_event.c
    third_party/wslay/lib/wslay_frame.c
    third_party/wslay/lib/wslay_net.c
    third_party/wslay/lib/wslay_queue.c)
  target_include_directories(wslay
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/wslay/lib/includes
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/third_party/wslay/lib
      ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_definitions(wslay
    PRIVATE HAVE_CONFIG_H
    PUBLIC WSLAY_VERSION="${ARIA2_WSLAY_VERSION}")
endif()

if(ENABLE_LIBARIA2)
  add_library(aria2_core ${ARIA2_CORE_SOURCES})
  set_target_properties(aria2_core PROPERTIES OUTPUT_NAME aria2)
else()
  add_library(aria2_core STATIC ${ARIA2_CORE_SOURCES})
endif()

target_compile_definitions(aria2_core PUBLIC HAVE_CONFIG_H)
target_include_directories(aria2_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/includes
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/src/includes
  PRIVATE
    ${ARIA2_BOOST_ROOT})
target_compile_definitions(aria2_core PRIVATE
  BOOST_ASIO_HEADER_ONLY
  BOOST_ERROR_CODE_HEADER_ONLY)
target_link_libraries(aria2_core PRIVATE spdlog::spdlog_header_only)
target_link_libraries(aria2_core PUBLIC CURL::libcurl_static)
target_link_libraries(aria2_core PUBLIC aria2::gpac aria2::avformat
  aria2::avcodec aria2::swresample aria2::avutil)
if(ENABLE_WEBSOCKET)
  target_link_libraries(aria2_core PUBLIC wslay)
endif()
if(HAVE_ZLIB)
  target_link_libraries(aria2_core PUBLIC aria2::zlib)
endif()
if(HAVE_LIBEXPAT)
  target_link_libraries(aria2_core PUBLIC aria2::expat)
endif()
if(HAVE_SQLITE3)
  target_link_libraries(aria2_core PUBLIC aria2::sqlite)
endif()
if(ENABLE_BITTORRENT)
  target_link_libraries(aria2_core PUBLIC
    LibtorrentRasterbar::torrent-rasterbar)
endif()
target_link_libraries(aria2_core PUBLIC OpenSSL::Crypto)
if(HAVE_OPENSSL)
  target_link_libraries(aria2_core PUBLIC OpenSSL::SSL)
endif()
if(APPLE AND HAVE_OPENSSL)
  target_link_libraries(aria2_core PUBLIC
    "-framework CoreFoundation"
    "-framework Security")
endif()
if(APPLE)
  target_link_libraries(aria2_core PUBLIC "-framework Foundation")
endif()
if(WIN32)
  # Asio lock layouts must agree across the core, dependencies and consumers.
  target_compile_definitions(aria2_core PUBLIC
    WINVER=${ARIA2_WINDOWS_VERSION}
    _WIN32_WINNT=${ARIA2_WINDOWS_VERSION})
  target_link_libraries(aria2_core PUBLIC ws2_32 wsock32 gdi32 winmm iphlpapi psapi crypt32 secur32 advapi32 shell32 ole32)
endif()

if(ARIA2_ENABLE_WERROR)
  target_compile_options(aria2_core PRIVATE -Werror)
endif()

add_executable(aria2-next src/main.cc)
target_link_libraries(aria2-next PRIVATE aria2_core spdlog::spdlog_header_only)

if(ARIA2_RELEASE_SIZE_OPTIMIZED)
  set(ARIA2_RELEASE_TARGETS aria2_core aria2-next)
  if(TARGET wslay)
    list(APPEND ARIA2_RELEASE_TARGETS wslay)
  endif()
  foreach(target ${ARIA2_RELEASE_TARGETS})
    target_compile_options(${target} PRIVATE
      $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Os>
      $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-ffunction-sections>
      $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-fdata-sections>
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Os>
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-ffunction-sections>
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-fdata-sections>)
  endforeach()

  if(APPLE)
    target_link_options(aria2-next PRIVATE -Wl,-dead_strip)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_link_options(aria2-next PRIVATE -Wl,--gc-sections)
  endif()
endif()

if(ARIA2_RELEASE_LTO)
  check_ipo_supported(RESULT aria2_ipo_supported OUTPUT aria2_ipo_output LANGUAGES C CXX)
  if(aria2_ipo_supported)
    set(ARIA2_LTO_TARGETS aria2_core aria2-next)
    if(TARGET wslay)
      list(APPEND ARIA2_LTO_TARGETS wslay)
    endif()
    set_property(TARGET ${ARIA2_LTO_TARGETS} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(WARNING "ARIA2_RELEASE_LTO requested but IPO is not supported: ${aria2_ipo_output}")
  endif()
endif()

install(TARGETS aria2-next RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(ENABLE_LIBARIA2)
  install(TARGETS aria2_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(DIRECTORY src/includes/aria2 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/src/libaria2.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()
