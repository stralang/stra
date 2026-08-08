cmake_minimum_required(VERSION 3.30)

project(common LANGUAGES CXX)
add_library(common STATIC lib.cpp)

target_include_directories(common PUBLIC ./)
