cmake_minimum_required(VERSION 3.16)
project(safe_steps CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(safe_steps safe_steps_styled.cpp)

# The server uses threads
find_package(Threads REQUIRED)
target_link_libraries(safe_steps Threads::Threads)
