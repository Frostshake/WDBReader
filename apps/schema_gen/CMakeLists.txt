cmake_minimum_required (VERSION 3.14)

add_executable(schema_gen main.cpp)
target_compile_features(schema_gen PRIVATE cxx_std_20)

target_link_libraries(schema_gen PRIVATE WDBReader)
target_compile_definitions(schema_gen PRIVATE STORMLIB_NO_AUTO_LINK CASCLIB_NO_AUTO_LINK_LIBRARY)
set_target_properties(schema_gen PROPERTIES DEBUG_POSTFIX "d")

if (MSVC)
    target_compile_definitions(schema_gen PUBLIC UNICODE _UNICODE)
endif()

install(TARGETS schema_gen
    RUNTIME DESTINATION bin
)