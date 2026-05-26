cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(
  aria2
  VERSION 2.2.4
  DESCRIPTION "High speed download utility"
  HOMEPAGE_URL "https://github.com/AnInsomniacy/aria2-next"
  LANGUAGES C CXX)

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
include(CheckTypeSize)
include(CMakePushCheckState)
include(GNUInstallDirs)
include(TestBigEndian)

find_package(PkgConfig)

include(cmake/modules/Options.cmake)
include(cmake/modules/FeatureChecks.cmake)
include(cmake/modules/GeneratedFiles.cmake)
include(cmake/modules/Targets.cmake)
include(cmake/modules/Tests.cmake)
include(cmake/modules/Summary.cmake)
