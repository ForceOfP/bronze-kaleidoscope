cmake_minimum_required(VERSION 3.0.0)
project(kalei VERSION 0.1.0)

include(CTest)
enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(kalei main.cpp)

aux_source_directory(. DIR_SRCS)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_subdirectory("src/ast")

# Find the installed re2 library
# find_package(re2 REQUIRED)

target_link_libraries(
    kalei
    AST

    # re2 should be put at bottom
    re2
)