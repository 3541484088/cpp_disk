cmake_minimum_required(VERSION 3.20)
project(xdisk_pb LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(xdisk_pb STATIC ${SOURCES})

target_include_directories(xdisk_pb
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${XDISK_PROTOBUF_DIR}/include
)

target_link_libraries(xdisk_pb PUBLIC
    xplatform
    ${XDISK_PROTOBUF_LIB}
)

set_target_properties(xdisk_pb PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib
)
