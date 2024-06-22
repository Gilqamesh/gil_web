cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(lib_name gilweb)

project(${lib_name} VERSION 0.1)
add_library(${lib_name} STATIC web.cpp)
target_include_directories(${lib_name} PUBLIC "${PROJECT_BINARY_DIR}")
configure_file(web.h.in web.h)

