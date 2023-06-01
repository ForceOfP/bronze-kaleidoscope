
#set(CPACK_PROJECT_NAME ${PROJECT_NAME})
#set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
#include(CPack)

#aux_source_directory(ast AST_SRCS)

#option(UT "Enable unit tests" OFF)
#
#if (UT) 
#    add_definitions(-DUT)
#endif()
#
#message(STATUS "Enable testing: ${UT}")

include_directories(ast)
aux_source_directory(./ast AST_SRCS)

add_library(AST ${AST_SRCS})
add_executable(kalei main.cpp)

target_link_libraries(
    kalei
    AST

    # re2 should be put at bottom
    re2
)