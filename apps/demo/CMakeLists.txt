cmake_minimum_required (VERSION 3.14)

add_executable(demo main.cpp)
target_compile_features(demo PRIVATE cxx_std_20)

target_link_libraries(demo PRIVATE WDBReader)
target_compile_definitions(demo PRIVATE STORMLIB_NO_AUTO_LINK CASCLIB_NO_AUTO_LINK_LIBRARY)

if (MSVC)
     target_compile_definitions(demo PUBLIC UNICODE _UNICODE)
endif()