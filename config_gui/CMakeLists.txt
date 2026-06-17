cmake_minimum_required(VERSION 3.20)
project(config_gui LANGUAGES CXX)

include(../cmake/XdiskCommon.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets
        PATHS ${XDISK_QT_DIR} NO_DEFAULT_PATH)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
endif()

file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.qrc"
)

if(WIN32)
    add_executable(config_gui WIN32 ${SOURCES})
else()
    add_executable(config_gui ${SOURCES})
endif()

target_include_directories(config_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XDISK_ENV_ROOT}/include
)

target_link_libraries(config_gui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    xplatform
    config_client
    xauth_client
)

set_target_properties(config_gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)
