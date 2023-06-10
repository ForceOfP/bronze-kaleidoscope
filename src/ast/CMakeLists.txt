project(ast)

file(GLOB AST_SRCS "*.cpp")

add_library(${PROJECT_NAME} ${AST_SRCS})
add_library(AST ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} 
    PUBLIC ${project_SOURCE_DIR}
    PUBLIC ${kalei_SOURCE_DIR}/src
)