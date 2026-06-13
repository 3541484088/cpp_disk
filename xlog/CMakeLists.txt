cmake_minimum_required(VERSION 3.20)
project(xlog_server LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(xlog_server ${SOURCES})

target_include_directories(xlog_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XDISK_ENV_ROOT}/include
    ${XDISK_MYSQL_DIR}/include
)

target_compile_definitions(xlog_server PRIVATE
    STATIC
)
if(WIN32)
    target_compile_definitions(xlog_server PRIVATE _CONSOLE)
endif()

target_link_libraries(xlog_server PRIVATE
    xplatform
    register_client
    LXMysql
)

set_target_properties(xlog_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

xdisk_copy_runtime_dlls(xlog_server)
