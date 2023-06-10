project(utils)

add_library(${PROJECT_NAME} INTERFACE)
add_library(UTILS ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    INTERFACE ${PROJECT_SOURCE_DIR}
)