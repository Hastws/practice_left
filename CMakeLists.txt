cmake_minimum_required(VERSION 3.16)
project(LeftHandTrainer LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

add_executable(LeftHandTrainer
        main.cpp
        trainer_window.cpp
        trainer_window.h
)

target_link_libraries(LeftHandTrainer PRIVATE Qt6::Widgets)
