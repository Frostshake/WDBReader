cmake_minimum_required (VERSION 3.14)

file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${WDBReader_SOURCE_DIR}/include/WDBReader/*.hpp" "${WDBReader_SOURCE_DIR}/include/WDBReader/Database/*.hpp" "${WDBReader_SOURCE_DIR}/include/WDBReader/Filesystem/*.hpp")
set(SOURCE_LIST Detection.cpp WoWDBDefs.cpp)

if (CascLib_FOUND)
    list(APPEND HEADER_LIST "${WDBReader_SOURCE_DIR}/include/WDBReader/Filesystem/CASCFilesystem.hpp")
    list(APPEND SOURCE_LIST Filesystem/CASCFilesystem.cpp)
endif()

if (StormLib_FOUND)
    list(APPEND HEADER_LIST "${WDBReader_SOURCE_DIR}/include/WDBReader/Filesystem/MPQFilesystem.hpp")
    list(APPEND SOURCE_LIST Filesystem/MPQFilesystem.cpp)
endif()

add_library(WDBReader ${SOURCE_LIST} ${HEADER_LIST})
add_library(WDBReader::WDBReader ALIAS WDBReader)

set_target_properties(WDBReader PROPERTIES DEBUG_POSTFIX "d")

target_include_directories(WDBReader 
    PUBLIC      
    $<BUILD_INTERFACE:${WDBReader_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(WDBReader PUBLIC cxx_std_20)

if(WIN32)
    target_link_libraries(WDBReader PRIVATE propsys.lib)
endif()

if (CascLib_FOUND)
    target_compile_definitions(WDBReader PRIVATE CASCLIB_NO_AUTO_LINK_LIBRARY)
    target_include_directories(WDBReader PUBLIC ${CascLib_INCLUDE_DIRS})
    target_link_libraries(WDBReader PUBLIC CascLib::casc)
endif()

if (StormLib_FOUND)
    target_compile_definitions(WDBReader PRIVATE STORMLIB_NO_AUTO_LINK)
    target_include_directories(WDBReader PUBLIC ${StormLib_INCLUDE_DIRS})
    target_link_libraries(WDBReader PUBLIC StormLib::storm)
endif()

install(TARGETS WDBReader
        EXPORT WDBReaderTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )

install(FILES ${HEADER_LIST} DESTINATION include)