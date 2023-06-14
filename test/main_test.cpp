#include <gtest/gtest.h> 
#include "ast/lexer_test.hpp"
#include "ast/parser_test.hpp"
#include "codegen/codegen_test.hpp"

int main() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}