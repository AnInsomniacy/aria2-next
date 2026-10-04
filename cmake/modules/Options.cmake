set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(WIN32)
  # Match libtorrent's Windows baseline before any system or Asio headers.
  set(ARIA2_WINDOWS_VERSION 0x0A00)
  list(APPEND CMAKE_REQUIRED_DEFINITIONS
    -DWINVER=${ARIA2_WINDOWS_VERSION}
    -D_WIN32_WINNT=${ARIA2_WINDOWS_VERSION})
endif()

option(ARIA2_ENABLE_BITTORRENT "Enable BitTorrent support" ON)
option(ARIA2_ENABLE_METALINK "Enable Metalink support" ON)
option(ARIA2_ENABLE_WEBSOCKET "Enable WebSocket support" ON)
option(ARIA2_ENABLE_EPOLL "Enable epoll support where available" ON)
option(ARIA2_ENABLE_LIBARIA2 "Build and install libaria2" OFF)
option(ARIA2_ENABLE_WERROR "Treat compiler warnings as errors" OFF)
option(ARIA2_RELEASE_SIZE_OPTIMIZED "Optimize release binaries for size and linker garbage collection" OFF)
option(ARIA2_RELEASE_LTO "Enable interprocedural optimization for release builds when supported" OFF)

set(ARIA2_DEFAULT_DISK_CACHE "" CACHE STRING "Default disk cache size")
set(ARIA2_BASH_COMPLETION_DIR "" CACHE PATH "Bash completion installation directory")
set(ARIA2_DEPENDENCY_ROOT "" CACHE PATH "Vendored dependency installation")
set(ARIA2_BOOST_ROOT "" CACHE PATH "Vendored Boost source root")

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/dependencies.env"
  ARIA2_WSLAY_VERSION_LINE REGEX "^WSLAY_VERSION=[0-9]+\\.[0-9]+\\.[0-9]+$")
string(REGEX REPLACE "^WSLAY_VERSION=" ""
  ARIA2_WSLAY_VERSION "${ARIA2_WSLAY_VERSION_LINE}")
if(NOT ARIA2_WSLAY_VERSION)
  message(FATAL_ERROR "WSLAY_VERSION is missing from packaging/dependencies.env")
endif()

if(NOT IS_DIRECTORY "${ARIA2_DEPENDENCY_ROOT}")
  message(FATAL_ERROR "ARIA2_DEPENDENCY_ROOT must reference the vendored dependency installation")
endif()
if(NOT IS_DIRECTORY "${ARIA2_BOOST_ROOT}/boost/asio")
  message(FATAL_ERROR "ARIA2_BOOST_ROOT must reference the vendored Boost source root")
endif()

function(aria2_check_include header variable)
  check_include_file("${header}" "${variable}")
endfunction()

function(aria2_check_c_symbol symbol variable)
  set(_headers ${ARGN})
  check_symbol_exists("${symbol}" "${_headers}" "${variable}")
endfunction()

function(aria2_check_c_compiles variable source)
  check_c_source_compiles("${source}" "${variable}")
endfunction()

set(ARIA2_MIN_OPENSSL_VERSION 3.0.0)
set(ARIA2_MIN_LIBTORRENT_VERSION 2.1.1)
