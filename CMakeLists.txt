cmake_minimum_required(VERSION 3.1.0)

project(ble_connector)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

find_package(Qt5 COMPONENTS Core Widgets Bluetooth REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/bluetooth_connector.cpp
    src/bluetooth_connector.h
)

target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Widgets
    Qt5::Bluetooth
)
