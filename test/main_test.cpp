#include <gtest/gtest.h> 
#include "ast/lexer_test.hpp"
#include "ast/parser_test.hpp"

int main() {
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}