cmake_minimum_required(VERSION 3.16)
project(task_12 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(task_12
    src/main.cpp
)

target_link_libraries(task_12 PRIVATE Qt6::Widgets)

if(MSVC)
    target_compile_options(task_12 PRIVATE /W4)
else()
    target_compile_options(task_12 PRIVATE -Wall -Wextra -pedantic)
endif()
