cmake_minimum_required (VERSION 3.14)

file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${WDBReader_SOURCE_DIR}/include/WDBReader/*.hpp" "${WDBReader_SOURCE_DIR}/include/WDBReader/Database/*.hpp" "${WDBReader_SOURCE_DIR}/include/WDBReader/Filesystem/*.hpp")
set(SOURCE_LIST Detection.cpp WoWDBDefs.cpp)

if (CASCLIB_DIR)
    list(APPEND HEADER_LIST "${WDBReader_SOURCE_DIR}/include/WDBReader/Filesystem/CASCFilesystem.hpp")
    list(APPEND SOURCE_LIST Filesystem/CASCFilesystem.cpp)
endif()

if (STORMLIB_DIR)
    list(APPEND HEADER_LIST "${WDBReader_SOURCE_DIR}/include/WDBReader/Filesystem/MPQFilesystem.hpp")
    list(APPEND SOURCE_LIST Filesystem/MPQFilesystem.cpp)
endif()

add_library(WDBReader ${SOURCE_LIST} ${HEADER_LIST})
add_library(WDBReader::WDBReader ALIAS WDBReader)

target_include_directories(WDBReader PUBLIC ../include)
target_compile_features(WDBReader PUBLIC cxx_std_20)

if(WIN32)
    target_link_libraries(WDBReader PRIVATE propsys.lib)
endif()

if (CASCLIB_DIR)
    target_compile_definitions(WDBReader PRIVATE CASCLIB_NO_AUTO_LINK_LIBRARY)
    target_include_directories(WDBReader PUBLIC ${CASCLIB_DIR}/release/include)
    target_link_libraries(WDBReader PUBLIC debug ${CASCLIB_DIR}/debug/lib/casc.lib)
    target_link_libraries(WDBReader PUBLIC optimized ${CASCLIB_DIR}/release/lib/casc.lib)
endif()

if (STORMLIB_DIR)
    target_compile_definitions(WDBReader PRIVATE STORMLIB_NO_AUTO_LINK)
    target_include_directories(WDBReader PUBLIC ${STORMLIB_DIR}/release/include)
    target_link_libraries(WDBReader PUBLIC debug ${STORMLIB_DIR}/debug/lib/StormLib.lib)
    target_link_libraries(WDBReader PUBLIC optimized ${STORMLIB_DIR}/release/lib/StormLib.lib)
endif()

