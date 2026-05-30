cmake_minimum_required(VERSION 3.20)
project(register_server LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(register_server ${SOURCES})

target_include_directories(register_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XDISK_ENV_ROOT}/include
)

target_compile_definitions(register_server PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _CONSOLE
)

target_link_libraries(register_server PRIVATE xplatform)

set_target_properties(register_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

xdisk_copy_runtime_dlls(register_server)
