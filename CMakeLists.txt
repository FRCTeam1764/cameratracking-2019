cmake_minimum_required(VERSION 3.1)

OPTION(DEBUG "DEBUG" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

if(DEBUG)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
else()
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")
endif()

set (PROJECT_NAME my_test)

project(${PROJECT_NAME})

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(src)
add_executable(${PROJECT_NAME} src/main.cpp src/cli/flag_interpreter.cpp src/network/network.cpp)
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS} )


#SET(OpenCV DIR /home/ubuntu/
