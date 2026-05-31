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
    _CRT_SECURE_NO_WARNINGS
    WIN32
    _WINDOWS
    EVENT__HAVE_OPENSSL
    _HAS_STD_BYTE=0
)

# 源文件为 UTF-8 编码，强制 MSVC 按 UTF-8 解析（否则 GBK 代码页下会误解析中文注释）
# /FS 选项解决多个 CL.EXE 同时写入同一个 PDB 文件的问题
if(MSVC)
    target_compile_options(xplatform PRIVATE /utf-8 /FS)
endif()

target_link_libraries(xplatform
    PUBLIC
        # protobuf 通过 xplatform 公开头文件（xmsg_com.pb.h 等）暴露给所有消费者
        # 消费者链接 xplatform 时，链接器也必须能找到 libprotobuf 符号
        ${XDISK_PROTOBUF_LIB}
    PRIVATE
        ${XDISK_LIBEVENT_LIB_DIR}/libevent.lib
        ${XDISK_OPENSSL_LIB_DIR}/libssl.lib
        ${XDISK_OPENSSL_LIB_DIR}/libcrypto.lib
        ${XDISK_LIBEVENT_LIB_DIR}/libevent_openssl.lib
        ws2_32
)

set_target_properties(xplatform PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib
)
