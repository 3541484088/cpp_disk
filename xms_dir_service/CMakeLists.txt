cmake_minimum_required(VERSION 3.20)
project(xms_dir_service LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(xms_dir_service ${SOURCES})

target_include_directories(xms_dir_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XDISK_ENV_ROOT}/include
)

target_compile_definitions(xms_dir_service PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _CONSOLE
)

target_link_libraries(xms_dir_service PRIVATE
    xplatform
    register_client
    xdisk_pb
)

set_target_properties(xms_dir_service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

xdisk_copy_runtime_dlls(xms_dir_service)
