cmake_minimum_required(VERSION 3.20)
project(xms_disk_client_gui LANGUAGES CXX)

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
    add_executable(xms_disk_client_gui WIN32 ${SOURCES})
else()
    add_executable(xms_disk_client_gui ${SOURCES})
endif()

target_include_directories(xms_disk_client_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/xauth
    ${CMAKE_SOURCE_DIR}/xms_dir_service
    ${CMAKE_SOURCE_DIR}/xdisk_pb
    ${XDISK_ENV_ROOT}/include
)

target_link_libraries(xms_disk_client_gui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    xplatform
    xauth_client
    xdisk_pb
)

set_target_properties(xms_disk_client_gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)
