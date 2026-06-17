cmake_minimum_required(VERSION 3.20)
project(xplatform LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)
list(FILTER SOURCES EXCLUDE REGEX "watchdog\\.cpp$")

add_library(xplatform STATIC ${SOURCES})

target_include_directories(xplatform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${XDISK_PROTOBUF_DIR}/include   # pb 头文件需传递给消费者
    PRIVATE
        ${XDISK_LIBEVENT_DIR}/include
        ${XDISK_OPENSSL_DIR}/include
        ${XDISK_MYSQL_DIR}/include
        ${XDISK_ENV_ROOT}/include
)

target_compile_definitions(xplatform
    PUBLIC XCOM_STATIC
    PRIVATE
        EVENT__HAVE_OPENSSL
        _HAS_STD_BYTE=0
)

# Windows 专用: WIN32 + CRT 安全宏 已在 XdiskCommon 中全局添加 _CRT_SECURE_NO_WARNINGS
if(WIN32)
    target_compile_definitions(xplatform PRIVATE WIN32 _WINDOWS)
endif()

# MSVC: UTF-8 源文件解析 + PDB 锁定
if(MSVC)
    target_compile_options(xplatform PRIVATE /utf-8 /FS)
endif()

# ============================================================
# 链接库 — 跨平台
# ============================================================
if(WIN32)
    target_link_libraries(xplatform
        PUBLIC
            ${XDISK_PROTOBUF_LIB}
        PRIVATE
            ${XDISK_LIBEVENT_LIB_DIR}/libevent.lib
            ${XDISK_OPENSSL_LIB_DIR}/libssl.lib
            ${XDISK_OPENSSL_LIB_DIR}/libcrypto.lib
            ${XDISK_LIBEVENT_LIB_DIR}/libevent_openssl.lib
            ws2_32
    )
else()
    target_link_libraries(xplatform
        PUBLIC
            ${XDISK_PROTOBUF_LIB}
        PRIVATE
            event
            event_openssl
            ssl
            crypto
            mysqlclient
            pthread
    )
endif()

set_target_properties(xplatform PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY         ${CMAKE_BINARY_DIR}/lib
)
