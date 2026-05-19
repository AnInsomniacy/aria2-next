cmake_minimum_required(VERSION 3.25)

project(
  aria2
  VERSION 2.1.6
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
