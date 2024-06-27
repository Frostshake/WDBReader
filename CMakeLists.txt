cmake_minimum_required (VERSION 3.14)

project(
  "WDBReader"
  VERSION 1.0.0
  DESCRIPTION "C++ utilities for reading wow db files."
  LANGUAGES CXX
)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

  if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
  endif()

  set(CMAKE_CXX_EXTENSIONS OFF)

  set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  include(CTest)
  enable_testing()

  if(BUILD_TESTING)
    set(TESTING_WOWDBDEFS_DIR "" CACHE PATH "WoWDBDefs test directory")
    add_subdirectory(test)
  endif()

  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT schema_gen)

  set(TESTING_CASC_DIR "" CACHE PATH "CASC test directory")
  set(TESTING_MPQ_DIR "" CACHE PATH "MPQ test directory")

endif()

set(CASCLIB_DIR "" CACHE PATH "CascLib directory")
set(STORMLIB_DIR "" CACHE PATH "StormLib directory")

add_subdirectory(src)

set(BUILD_APPS OFF CACHE BOOL "Build apps")

if(BUILD_APPS)
  add_subdirectory(apps)
endif()