project(codegen)

file(GLOB CODEGEN_SRCS "*.cpp")

add_library(${PROJECT_NAME} ${CODEGEN_SRCS})
add_library(CODEGEN ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} 
    PUBLIC ${project_SOURCE_DIR}
    PUBLIC ${kalei_SOURCE_DIR}/src
)