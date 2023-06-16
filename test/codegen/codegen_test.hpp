#pragma once

#include <gtest/gtest.h>
#include <llvm/Support/raw_ostream.h>
#include <vector>

#include "ast/ast.hpp"
#include "ast/lexer.hpp"
#include "ast/parser.hpp"
#include "codegen/codegen.hpp"

void codegen_helper(std::vector<std::string>& target, std::vector<std::string>& answer) {
    std::string ans = "";
    llvm::raw_string_ostream output(ans); 
    auto generator = CodeGenerator(output);
    generator.setting_.print_ir = false;
    for (int i = 0; i < target.size(); i++) {
        auto parser = Parser(Lexer::tokenize(target[i]), generator.binary_oper_precedence_);
        auto asts = parser.parse();
        generator.codegen(std::move(asts));
        ASSERT_EQ(ans, answer[i]);
        ans.clear();
    }
}

TEST(CODEGEN, single) {
    std::vector<std::string> target = {
        "def f(x) x + x",
        "extern sin(x)",
        "2 + 3"
    };

    std::vector<std::string> answer = {
        "parsed definition.\n",
        "declare double @sin(double)\n",
        "5.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, optimize) {
    std::vector<std::string> target = {
        "def g(x) 2 + 3 + x", // TODO(lbt): "x + 2 + 3" will break the constant folding, check what happened.
        "def h(x, y) x * y + y * x",
        "def i(x) (1 + 2 + x)*(x + (1 + 2));",
    };

    std::vector<std::string> answer = {
        "parsed definition.\n",
        "parsed definition.\n",
        "parsed definition.\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, jit) {
    std::vector<std::string> target = {
        "def f(x, y, z) x * y + z",
        "f(4, 2, 3)",
        "f(1, 2, 3)"
    };

    std::vector<std::string> answer = {
        "parsed definition.\n",
        "11.000000\n",
        "5.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);    
}

TEST(CODEGEN, ext) {
    std::vector<std::string> target = {
        "extern sin(x)",
        "extern cos(x)",
        "def f(x) sin(x) * sin(x) + cos(x) * cos(x)",
        "f(1.0)",
        "f(4.0)"
    };

    std::vector<std::string> answer = {
        "declare double @sin(double)\n",
        "declare double @cos(double)\n",
        // TODO(lbt): there are still some folding problem...
        "parsed definition.\n",
        "1.000000\n",
        "1.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, ifelse) {
    std::vector<std::string> target = {
        "def double(x) x + x",
        "def triple(x) x * 3",
        "def f(x) if x < 3 then double(x) else triple(x);",
        "f(2)",
        "f(5)"
    };

    std::vector<std::string> answer = {
        "parsed definition.\n",
        "parsed definition.\n",
        "parsed definition.\n",
        "4.000000\n",
        "15.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, loop) {
    std::vector<std::string> target = {
        "extern putchard(char)",
        "def printstar(n) for i = 0, i < n, 1.0 in putchard(42);",
        "printstar(20)",
        "printstar(30)"
    };

    std::vector<std::string> answer = {
        "declare double @putchard(double)\n",
        "parsed definition.\n",
        // TODO(lbt): this example should be updated.
        "0.000000\n",
        "0.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}
