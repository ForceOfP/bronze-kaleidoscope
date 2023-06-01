cmake_minimum_required(VERSION 3.0.0)
project(kalei VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src)
add_subdirectory(src/ast)

enable_testing()
add_subdirectory(test)