cmake_minimum_required(VERSION 3.20)
project(xms_add_user LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(xms_add_user ${SOURCES})

target_include_directories(xms_add_user PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/xauth
    ${XDISK_ENV_ROOT}/include
)

# _CRT_SECURE_NO_WARNINGS 已在 XdiskCommon.cmake 中全局配置
if(WIN32)
    target_compile_definitions(xms_add_user PRIVATE _CONSOLE)
endif()

target_link_libraries(xms_add_user PRIVATE
    xauth_client
)

set_target_properties(xms_add_user PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)