add_executable(kalei main.cpp)

target_link_libraries(
    kalei
    AST
    UTILS

    # re2 should be put at bottom
    re2
)