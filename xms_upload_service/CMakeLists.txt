cmake_minimum_required(VERSION 3.20)
project(xms_upload_service LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(xms_upload_service ${SOURCES})

target_include_directories(xms_upload_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XDISK_ENV_ROOT}/include
)

# _CRT_SECURE_NO_WARNINGS 已在 XdiskCommon.cmake 中全局配置
if(WIN32)
    target_compile_definitions(xms_upload_service PRIVATE _CONSOLE)
endif()

target_link_libraries(xms_upload_service PRIVATE
    xplatform
    register_client
    xdisk_pb
)

set_target_properties(xms_upload_service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

xdisk_copy_runtime_dlls(xms_upload_service)
