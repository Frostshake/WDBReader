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

  if(BUILD_APPS)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT schema_gen)
  endif()

  set(TESTING_CASC_DIR "" CACHE PATH "CASC test directory")
  set(TESTING_MPQ_DIR "" CACHE PATH "MPQ test directory")

endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake")
  # stormlib encounters issues finding these when build through vcpkg, unsure why, but this solves it.
  find_package(BZip2 REQUIRED)
  find_package(ZLIB REQUIRED)
endif()

find_package(StormLib CONFIG)
find_package(CascLib CONFIG)

add_subdirectory(src)

set(BUILD_APPS OFF CACHE BOOL "Build apps")

if(BUILD_APPS)
  add_subdirectory(apps)
endif()

set(VERSION_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/WDBReaderConfigVersion.cmake")
set(PROJECT_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/WDBReaderConfig.cmake")

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${VERSION_CONFIG}"
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY AnyNewerVersion
    )

configure_package_config_file("Config.cmake.in"
    "${PROJECT_CONFIG}"
    INSTALL_DESTINATION "lib/cmake/WDBReader")

install(FILES "${PROJECT_CONFIG}" "${VERSION_CONFIG}" DESTINATION "lib/cmake/WDBReader")
install(EXPORT WDBReaderTargets NAMESPACE WDBReader:: DESTINATION "lib/cmake/WDBReader")
