cmake_minimum_required(VERSION 3.20)
project(xai_lib LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

set(CMAKE_AUTOMOC ON)

if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Core
        PATHS ${XDISK_QT_DIR} NO_DEFAULT_PATH)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core)
endif()

set(SOURCES
    xai_assistant.cpp
    xai_assistant.h
    xai_tool_registry.cpp
    xai_tool_registry.h
    xai_tools.cpp
    xai_tools.h
    xai_prompt_builder.cpp
    xai_prompt_builder.h
    xai_file_tags.cpp
    xai_file_tags.h
    xai_content_index.cpp
    xai_content_index.h
)

add_library(xai_lib STATIC ${SOURCES})

target_include_directories(xai_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/xdisk_pb
    ${XDISK_ENV_ROOT}/include
    ${CURL_INCLUDE_DIR}
    ${JSONCPP_INCLUDE_DIRS}
)

target_link_libraries(xai_lib PUBLIC
    Qt6::Core
    xdisk_pb
    ${CURL_LIBRARY}
    ${JSONCPP_LIBRARIES}
)
