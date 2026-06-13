# XdiskCommon.cmake
# 所有子模块通过 include(../cmake/XdiskCommon.cmake) 引入此文件
# 变量由 CMakePresets.json 的 cacheVariables 注入

# ============================================================
# 平台检测
# ============================================================
if(WIN32)
    set(XDISK_PLATFORM_WINDOWS TRUE)
elseif(UNIX)
    set(XDISK_PLATFORM_LINUX TRUE)
endif()

# ============================================================
# 必要变量校验（非GUI模块才需要 env 依赖）
# ============================================================
# 直接无条件校验，因为所有后端模块都需要这些库
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
set(XDISK_LIB_DIR ${XDISK_ENV_ROOT}/lib)

# ============================================================
# protobuf 库 — 跨平台
# ============================================================
if(WIN32)
    set(XDISK_PROTOBUF_LIB
        $<IF:$<CONFIG:Debug>,${XDISK_PROTOBUF_LIB_DIR}/libprotobufd.lib,${XDISK_PROTOBUF_LIB_DIR}/libprotobuf.lib>
    )
else()
    # Linux: 通过 pkg-config 或 find_package 查找
    find_package(Protobuf REQUIRED)
    set(XDISK_PROTOBUF_LIB protobuf::libprotobuf)
endif()

# ============================================================
# Copy runtime DLLs — Windows only
# ============================================================
function(xdisk_copy_runtime_dlls target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${XDISK_OPENSSL_DIR}/bin/libcrypto-1_1-x64.dll"
                "${XDISK_OPENSSL_DIR}/bin/libssl-1_1-x64.dll"
                "${XDISK_MYSQL_DIR}/lib/libmysql.dll"
                "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copy runtime DLLs for ${target}"
        )
    endif()
endfunction()

# ============================================================
# 编译器选项 — 跨平台
# ============================================================
if(MSVC)
    add_compile_options(/utf-8 /FS)
    # Windows MSVC 屏蔽安全警告
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    # Linux: 启用 pthread、常见 warning
    add_compile_options(-Wall -Wextra -pthread)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
