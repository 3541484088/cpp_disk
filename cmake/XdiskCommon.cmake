# XdiskCommon.cmake
# 所有子模块通过 include(../cmake/XdiskCommon.cmake) 引入此文件
# 变量由 CMakePresets.json 的 cacheVariables 注入

# Fail fast if required variables are not set
foreach(_var XDISK_ENV_ROOT XDISK_LIBEVENT_DIR XDISK_OPENSSL_DIR XDISK_PROTOBUF_DIR XDISK_MYSQL_DIR)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "XdiskCommon.cmake: required variable ${_var} is not set. Use a CMakePresets.json configurePreset.")
    endif()
endforeach()

set(XDISK_INCLUDE_DIRS
    ${XDISK_ENV_ROOT}/include
    ${XDISK_LIBEVENT_DIR}/include
    ${XDISK_OPENSSL_DIR}/include
    ${XDISK_PROTOBUF_DIR}/include
    ${XDISK_MYSQL_DIR}/include
)

# Per-component lib directories
set(XDISK_LIBEVENT_LIB_DIR  ${XDISK_LIBEVENT_DIR}/lib)
set(XDISK_OPENSSL_LIB_DIR   ${XDISK_OPENSSL_DIR}/lib)
set(XDISK_PROTOBUF_LIB_DIR  ${XDISK_PROTOBUF_DIR}/lib)
set(XDISK_MYSQL_LIB_DIR     ${XDISK_MYSQL_DIR}/lib)
# Fallback lib dir for shared/misc libs (libevent_openssl.lib etc.)
set(XDISK_LIB_DIR ${XDISK_ENV_ROOT}/lib)

# protobuf 库名：Debug 用 libprotobufd.lib，Release 用 libprotobuf.lib
set(XDISK_PROTOBUF_LIB
    $<IF:$<CONFIG:Debug>,${XDISK_PROTOBUF_LIB_DIR}/libprotobufd.lib,${XDISK_PROTOBUF_LIB_DIR}/libprotobuf.lib>
)

# Copy OpenSSL / MySQL runtime DLLs next to service executables after build
function(xdisk_copy_runtime_dlls target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${XDISK_OPENSSL_DIR}/bin/libcrypto-1_1-x64.dll"
            "${XDISK_OPENSSL_DIR}/bin/libssl-1_1-x64.dll"
            "${XDISK_MYSQL_DIR}/lib/libmysql.dll"
            "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Copy runtime DLLs for ${target}"
    )
endfunction()

# MSVC编译器选项：使用UTF-8编码处理源文件，/FS解决PDB文件锁定问题
if(MSVC)
    add_compile_options(/utf-8 /FS)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
