enable_testing()

include_directories(ast)
include_directories(codegen)
include_directories(${kalei_SOURCE_DIR}/src)

aux_source_directory(ast AST_TEST_DIR)
aux_source_directory(codegen CODEGEN_TEST_DIR)
set(
    ALL_TEST_FILES 
    main_test.cpp
    ${AST_TEST_DIR}
    ${CODEGEN_TEST_DIR}
)

add_executable(ut_test main_test.cpp ${ALL_TEST_FILES})
target_link_libraries(
    ut_test
    AST
    CODEGEN

    ${llvm_libs}
    gtest 
    re2
)

add_test(MyTest ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ut_test)