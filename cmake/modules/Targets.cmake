# Everything but main(), shared by the executable and the unit tests.
file(GLOB_RECURSE ARIA2_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")
list(REMOVE_ITEM ARIA2_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/app/Main.cc")

add_library(aria2_core STATIC ${ARIA2_SOURCES})
target_include_directories(aria2_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${ARIA2_BOOST_ROOT}")
target_compile_definitions(aria2_core PUBLIC
  ARIA2_VERSION="${PROJECT_VERSION}"
  BOOST_ASIO_HEADER_ONLY
  BOOST_ERROR_CODE_HEADER_ONLY
  BOOST_JSON_NO_LIB
  XML_STATIC)
target_link_libraries(aria2_core PUBLIC
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
  # Asio lock layouts must agree across the core, dependencies and consumers.
  target_compile_definitions(aria2_core PUBLIC
    WINVER=${ARIA2_WINDOWS_VERSION}
    _WIN32_WINNT=${ARIA2_WINDOWS_VERSION}
    WIN32_LEAN_AND_MEAN
    NOMINMAX)
  target_link_libraries(aria2_core PUBLIC
    ws2_32 mswsock iphlpapi crypt32 secur32 bcrypt advapi32 shell32 ole32 winmm)
endif()
if(APPLE)
  target_link_libraries(aria2_core PUBLIC
    "-framework CoreFoundation" "-framework Security" "-framework Foundation")
endif()
if(ARIA2_ENABLE_WERROR)
  target_compile_options(aria2_core PRIVATE -Werror)
endif()

add_executable(aria2-next "${CMAKE_CURRENT_SOURCE_DIR}/src/app/Main.cc")
target_link_libraries(aria2-next PRIVATE aria2_core)

if(ARIA2_RELEASE_SIZE_OPTIMIZED)
  foreach(target aria2_core aria2-next)
    target_compile_options(${target} PRIVATE
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
  check_ipo_supported(RESULT aria2_ipo_supported OUTPUT aria2_ipo_output LANGUAGES CXX)
  if(aria2_ipo_supported)
    set_property(TARGET aria2_core aria2-next PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(WARNING "ARIA2_RELEASE_LTO requested but IPO is not supported: ${aria2_ipo_output}")
  endif()
endif()

install(TARGETS aria2-next RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
