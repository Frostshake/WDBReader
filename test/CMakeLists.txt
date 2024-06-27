cmake_minimum_required (VERSION 3.14)

find_package(Catch2 3 REQUIRED)

add_executable(tests 
Tests.cpp 
DatabaseTest.cpp 
DatabaseDB2Test.cpp
DatabaseDBCTest.cpp
FilesystemTest.cpp 
WoWDBDefsTest.cpp
)

target_link_libraries(
  tests
  PRIVATE 
  WDBReader::WDBReader
  Catch2::Catch2WithMain
)

macro(set_normalized_path VARIABLE_NAME)
    if (${VARIABLE_NAME})
        file(TO_CMAKE_PATH "${${VARIABLE_NAME}}" ${VARIABLE_NAME}_NORMALIZED)
        target_compile_definitions(tests PRIVATE ${VARIABLE_NAME}="${${VARIABLE_NAME}_NORMALIZED}")
    endif()
endmacro()

set_normalized_path(TESTING_WOWDBDEFS_DIR)
set_normalized_path(TESTING_CASC_DIR)
set_normalized_path(TESTING_MPQ_DIR)

target_compile_definitions(tests PRIVATE STORMLIB_NO_AUTO_LINK CASCLIB_NO_AUTO_LINK_LIBRARY)

include(Catch)
catch_discover_tests(tests)

add_test(NAME test_tests COMMAND tests) 