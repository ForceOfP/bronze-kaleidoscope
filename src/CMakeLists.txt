add_executable(kalei main.cpp)

target_link_libraries(
    kalei
    AST
    UTILS
    CODEGEN

    ${llvm_libs}
    # re2 should be put at bottom
    re2
)