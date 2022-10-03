cmake_minimum_required(VERSION 3.0.0)
project(example)

set(CMAKE_CXX_STANDARD 20)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(example example.cpp thread_pool.hpp)

target_link_libraries(example PRIVATE Threads::Threads)
