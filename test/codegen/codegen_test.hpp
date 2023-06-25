#pragma once

#include <gtest/gtest.h>
#include <llvm/Support/raw_ostream.h>
#include <vector>

#include "ast/ast.hpp"
#include "ast/lexer.hpp"
#include "ast/parser.hpp"
#include "codegen/jit_codegen.hpp"

void codegen_helper(std::vector<std::string>& target, std::vector<std::string>& answer) {
    std::string ans = "";
    llvm::raw_string_ostream output(ans); 
    CodeGeneratorSetting setting = {
        .print_ir = false,
        .function_pass_optimize = true,
    };
    auto generator = JitCodeGenerator(output, setting);
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
        "def f(x) { return x + x; }",
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
        "def g(x) { return 2 + 3 + x; }", // TODO(lbt): "x + 2 + 3" will break the constant folding, check what happened.
        "def h(x, y) { return x * y + y * x; }",
        "def i(x) {return (1 + 2 + x)*(x + (1 + 2));};",
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
        "def f(x, y, z) {return x * y + z;}",
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
        "def f(x) {return sin(x) * sin(x) + cos(x) * cos(x);}",
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

TEST(CODEGEN, ifelse1) {
    std::vector<std::string> target = {
        "def double(x) {return x + x;}",
        "def triple(x) {return x * 3;}",
        "def f(x) {if (x < 3) {return double(x);} else {return triple(x);}};",
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

TEST(CODEGEN, ifelse2) {
    std::vector<std::string> target = {
        "def double(x) {return x + x;}",
        "def triple(x) {return x * 3;}",
        "def f(x) {return if (x < 3) {double(x)} else {triple(x)}};",
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
        "def binary : 1 (x, y) {return y;};",
        "def sum(n) {var a = 0, b = 0; for (i = 0, i < n) {b = a + b; a = a + 1;} return b; }",
        "sum(100)",
    };

    std::vector<std::string> answer = {
        "parsed definition.\n",
        "parsed definition.\n",
        "5050.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, oper) {
    std::vector<std::string> target = {
/*         "def unary - (v) 0 - v;",
        "-(2+3)", */
        "def unary ! (v) {if (v) {return 0;} else {return 1;}};",
        "!1",
        "!0",
        "def binary | 5 (LHS, RHS) {if (LHS) {return 1;} else {if (RHS) {return 1;} else {return 0;}}};",
        "0|0",
        "1|0",
        "def binary & 6 (LHS, RHS) {if (!LHS) {return 0;} else {return !(!RHS);}};",
        "1&0",
        "1&1",
        "def binary > 10 (LHS, RHS) {return RHS < LHS;};",
        "1 > 2",
        "2 > 1",
        "def binary == 9 (LHS, RHS) {return !(RHS < LHS | RHS > LHS);};",
        "1 == 0",
        "1 == 1",
    };

    std::vector<std::string> answer = {
/*         "parsed definition.\n",
        "-5.000000\n", */
        "parsed definition.\n",
        "0.000000\n",
        "1.000000\n",
        "parsed definition.\n",
        "0.000000\n",
        "1.000000\n",
        "parsed definition.\n",
        "0.000000\n",
        "1.000000\n",
        "parsed definition.\n",
        "0.000000\n",
        "1.000000\n",
        "parsed definition.\n",
        "0.000000\n",
        "1.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, variant) {
    std::vector<std::string> target = {
        "def binary : 1 (x, y) {return y;};",
        "def fibi(x) {var a = 1, b = 1, c; for (i = 3, i < x) {c = a + b; a = b; b = c;} return b;};",
        "fibi(10)",
    };

    std::vector<std::string> answer = {
        "parsed definition.\n",
        "parsed definition.\n",
        "55.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

