cmake_minimum_required(VERSION 3.20)
project(register_client LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(register_client STATIC ${SOURCES})

target_include_directories(register_client
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${XDISK_ENV_ROOT}/include
)

# _CRT_SECURE_NO_WARNINGS 已在 XdiskCommon.cmake 中全局配置

target_link_libraries(register_client PUBLIC xplatform)

set_target_properties(register_client PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib
)
