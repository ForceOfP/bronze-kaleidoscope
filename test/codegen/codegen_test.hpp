#pragma once

#include <gtest/gtest.h>
#include <llvm/Support/raw_ostream.h>

#include "ast/ast.hpp"
#include "ast/lexer.hpp"
#include "ast/parser.hpp"
#include "codegen/codegen.hpp"

TEST(CODEGEN, codegen) {
    std::vector<std::string> target = {
        "def f(x) x + x",
        "extern sin(x)",
        "2 + 3"
    };

    std::vector<std::string> answer = {
        "define double @f(double \%x) {\n"
        "entry:\n"
        "  \%addtmp = fadd double \%x, \%x\n"
        "  ret double \%addtmp\n"
        "}\n",
        "declare double @sin(double)\n",
        "define double @__anon_expr() {\n"
        "entry:\n"
        "  ret double 5.000000e+00\n"
        "}\n"
    };

    assert(target.size() == answer.size());
    for (int i = 0; i < target.size(); i++) {
        auto parser = Parser(Lexer::tokenize(target[i]));
        auto generator = CodeGenerator(parser.parse());
        std::string ans = "";
        llvm::raw_string_ostream output(ans);
        generator.codegen(output);
        ASSERT_EQ(ans, answer[i]);
    }
}