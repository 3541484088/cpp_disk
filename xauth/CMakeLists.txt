cmake_minimum_required(VERSION 3.20)
project(xauth LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

# xauth_client static lib (xauth_client.cpp only)
add_library(xauth_client STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/xauth_client.cpp
)

target_include_directories(xauth_client
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${XDISK_ENV_ROOT}/include
)

# _CRT_SECURE_NO_WARNINGS 已在 XdiskCommon.cmake 中全局配置

target_link_libraries(xauth_client PUBLIC xplatform)

set_target_properties(xauth_client PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib
)

# xauth_server executable (all cpp except xauth_client.cpp)
file(GLOB_RECURSE ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER ALL_SOURCES EXCLUDE REGEX "xauth_client\\.cpp$")

add_executable(xauth_server ${ALL_SOURCES})

target_include_directories(xauth_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XDISK_ENV_ROOT}/include
    ${XDISK_MYSQL_DIR}/include
)

if(WIN32)
    target_compile_definitions(xauth_server PRIVATE _CONSOLE)
endif()

target_link_libraries(xauth_server PRIVATE
    xauth_client
    config_client
    LXMysql
)

set_target_properties(xauth_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

xdisk_copy_runtime_dlls(xauth_server)
