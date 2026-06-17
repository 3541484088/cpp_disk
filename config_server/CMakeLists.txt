cmake_minimum_required(VERSION 3.20)
project(config_server LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(config_server ${SOURCES})

target_include_directories(config_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XDISK_ENV_ROOT}/include
    ${XDISK_MYSQL_DIR}/include
)

target_compile_definitions(config_server PRIVATE
    STATIC
)
if(WIN32)
    target_compile_definitions(config_server PRIVATE _CONSOLE)
endif()

target_link_libraries(config_server PRIVATE
    xplatform
    register_client
    LXMysql
)

set_target_properties(config_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

xdisk_copy_runtime_dlls(config_server)
