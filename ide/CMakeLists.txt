cmake_minimum_required(VERSION 3.18)



project(Starbytes-IDE CXX )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

add_executable("StarbytesIDE" "main.cpp")

target_link_libraries("StarbytesIDE" PRIVATE Qt6::Core Qt6::Widgets)