cmake_minimum_required(VERSION 3.20)
project(LXMysql LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(LXMysql STATIC ${SOURCES})

target_include_directories(LXMysql
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
            ${XDISK_MYSQL_DIR}/include
    PRIVATE ${XDISK_ENV_ROOT}/include
)

# Linux: mysql.h 在 /usr/include/mysql/ 子目录下
if(NOT WIN32)
    target_include_directories(LXMysql PUBLIC ${XDISK_MYSQL_DIR}/include/mysql)
endif()

target_compile_definitions(LXMysql PUBLIC
    STATIC
)

# 源文件为 UTF-8 编码，强制 MSVC 按 UTF-8 解析
# /FS 选项解决多个 CL.EXE 同时写入同一个 PDB 文件的问题
if(MSVC)
    target_compile_options(LXMysql PRIVATE /utf-8 /FS)
endif()

if(WIN32)
    target_link_libraries(LXMysql PUBLIC
        ${XDISK_MYSQL_DIR}/lib/libmysql.lib
    )
else()
    target_link_libraries(LXMysql PUBLIC
        mysqlclient
    )
endif()

set_target_properties(LXMysql PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib
)
