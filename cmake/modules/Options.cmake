set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
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

option(ARIA2_ENABLE_WERROR "Treat compiler warnings as errors" OFF)
option(ARIA2_RELEASE_SIZE_OPTIMIZED "Optimize release binaries for size and linker garbage collection" OFF)
option(ARIA2_RELEASE_LTO "Enable interprocedural optimization for release builds when supported" OFF)

set(ARIA2_BASH_COMPLETION_DIR "" CACHE PATH "Bash completion installation directory")
set(ARIA2_DEPENDENCY_ROOT "" CACHE PATH "Vendored dependency installation")
set(ARIA2_BOOST_ROOT "" CACHE PATH "Vendored Boost source root")

if(NOT IS_DIRECTORY "${ARIA2_DEPENDENCY_ROOT}")
  message(FATAL_ERROR "ARIA2_DEPENDENCY_ROOT must reference the vendored dependency installation")
endif()
if(NOT IS_DIRECTORY "${ARIA2_BOOST_ROOT}/boost/asio")
  message(FATAL_ERROR "ARIA2_BOOST_ROOT must reference the vendored Boost source root")
endif()
