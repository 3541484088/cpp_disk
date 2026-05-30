cmake_minimum_required(VERSION 3.20)
project(config_client LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(config_client STATIC ${SOURCES})

target_include_directories(config_client
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${XDISK_ENV_ROOT}/include
)

target_compile_definitions(config_client PRIVATE
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(config_client PUBLIC
    xplatform
    register_client
)

set_target_properties(config_client PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib
)
