cmake_minimum_required(VERSION 3.0.0)
project(example)

set (CMAKE_CXX_STANDARD 17)

add_executable(example example.cpp thread_pool.hpp)
