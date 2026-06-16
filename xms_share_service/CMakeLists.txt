cmake_minimum_required(VERSION 3.20)
project(xms_share_service LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(xms_share_service ${SOURCES})

target_include_directories(xms_share_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XDISK_ENV_ROOT}/include
    ../LXMysql
)

if(WIN32)
    target_compile_definitions(xms_share_service PRIVATE _CONSOLE)
endif()

target_link_libraries(xms_share_service PRIVATE
    xplatform
    register_client
    xdisk_pb
    LXMysql
)

set_target_properties(xms_share_service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

xdisk_copy_runtime_dlls(xms_share_service)
