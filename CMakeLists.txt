cmake_minimum_required(VERSION 3.25)

option(ARIA2_SUPERBUILD "Build the vendored dependency stack" ON)
option(ARIA2_ENABLE_BITTORRENT "Enable BitTorrent support" ON)
option(ARIA2_ENABLE_WEBSOCKET "Enable WebSocket support" ON)

if(ARIA2_SUPERBUILD)
  project(aria2-superbuild LANGUAGES C CXX)
  include(cmake/superbuild/Dependencies.cmake)
  return()
endif()

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

project(
  aria2
  VERSION 2.8.6
  DESCRIPTION "High speed download utility"
  HOMEPAGE_URL "https://github.com/AnInsomniacy/aria2-next"
  LANGUAGES C CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckCXXSourceCompiles)
include(CheckCXXSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckIPOSupported)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckSymbolExists)
include(CMakePushCheckState)
include(CTest)
include(GNUInstallDirs)
include(TestBigEndian)

include(cmake/modules/Options.cmake)
include(cmake/modules/Dependencies.cmake)
include(cmake/modules/FeatureChecks.cmake)
include(cmake/modules/GeneratedFiles.cmake)
include(cmake/modules/Targets.cmake)
if(BUILD_TESTING)
  include(cmake/modules/Tests.cmake)
endif()
include(cmake/modules/Summary.cmake)
