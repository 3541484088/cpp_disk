cmake_minimum_required(VERSION 3.20)
project(xms_gateway LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

if(WIN32)
    add_executable(xms_gateway
        ${SOURCES}
        ${XDISK_OPENSSL_DIR}/include/openssl/applink.c
    )
else()
    add_executable(xms_gateway ${SOURCES})
endif()

target_include_directories(xms_gateway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/xauth
    ${XDISK_ENV_ROOT}/include
    ${XDISK_OPENSSL_DIR}/include
)

# _CRT_SECURE_NO_WARNINGS 已在 XdiskCommon.cmake 中全局配置
if(WIN32)
    target_compile_definitions(xms_gateway PRIVATE _CONSOLE)
endif()

target_link_libraries(xms_gateway PRIVATE
    xplatform
    config_client
    register_client
)

set_target_properties(xms_gateway PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

xdisk_copy_runtime_dlls(xms_gateway)
