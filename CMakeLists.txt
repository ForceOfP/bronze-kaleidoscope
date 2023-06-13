cmake_minimum_required(VERSION 3.0.0)
project(kalei VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LLVM_DIR /usr/lib/llvm-15/lib/cmake/llvm)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(llvm_libs support core irreader)

add_subdirectory(src)
add_subdirectory(src/ast)
add_subdirectory(src/utils)
add_subdirectory(src/codegen)

enable_testing()
add_subdirectory(test)