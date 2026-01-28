cmake_minimum_required(VERSION 3.16)
project(LeftHandTrainer LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try Qt6 first, fall back to Qt5
find_package(Qt6 COMPONENTS Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.12 COMPONENTS Widgets REQUIRED)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

add_executable(LeftHandTrainer
        main.cpp
        trainer_window.cpp
        trainer_window.h
)

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(LeftHandTrainer PRIVATE Qt6::Widgets)
else()
    target_link_libraries(LeftHandTrainer PRIVATE Qt5::Widgets)
endif()
