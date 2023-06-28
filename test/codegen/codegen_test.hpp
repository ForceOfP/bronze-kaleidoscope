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
        "def f(x: double) { return x + x; }",
        "extern sin(x: double)",
        "exec 2 + 3"
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
        "def g(x: double) { return 2 + 3 + x; }", // TODO(lbt): "x + 2 + 3" will break the constant folding, check what happened.
        "def h(x: double, y: double) { return x * y + y * x; }",
        "def i(x: double) {return (1 + 2 + x)*(x + (1 + 2));};",
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
        "def f(x: double, y: double, z: double) {return x * y + z;}",
        "exec f(4, 2, 3)",
        "exec f(1, 2, 3)"
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
        "extern sin(x: double)",
        "extern cos(x: double)",
        "def f(x: double) {return sin(x) * sin(x) + cos(x) * cos(x);}",
        "exec f(1.0)",
        "exec f(4.0)"
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
        "def double(x: double) {return x + x;}",
        "def triple(x: double) {return x * 3;}",
        "def f(x: double) {if (x < 3) {return double(x);} else {return triple(x);}};",
        "exec f(2)",
        "exec f(5)"
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
        "def double(x: double) {return x + x;}",
        "def triple(x: double) {return x * 3;}",
        "def f(x: double) {return if (x < 3) {double(x)} else {triple(x)}};",
        "exec f(2)",
        "exec f(5)"
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
        "def sum(n: double) {var a: double = 0; var b: double = 0; for (i = 0, i < n) {b = a + b; a = a + 1;} return b; }",
        "exec sum(100)",
    };

    std::vector<std::string> answer = {
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
        "def unary ! (v: double) {if (v) {return 0;} else {return 1;}};",
        "exec !1",
        "exec !0",
        "def binary | 5 (LHS: double, RHS: double) {if (LHS) {return 1;} else {if (RHS) {return 1;} else {return 0;}}};",
        "exec 0|0",
        "exec 1|0",
        "def binary & 6 (LHS: double, RHS: double) {if (!LHS) {return 0;} else {return !(!RHS);}};",
        "exec 1&0",
        "exec 1&1",
        "def binary > 10 (LHS: double, RHS: double) {return RHS < LHS;};",
        "exec 1 > 2",
        "exec 2 > 1",
        "def binary == 9 (LHS: double, RHS: double) {return !(RHS < LHS | RHS > LHS);};",
        "exec 1 == 0",
        "exec 1 == 1",
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
        "def fibi(x: double) {var a: double = 1; var b: double = 1; var c: double; for (i = 3, i < x) {c = a + b; a = b; b = c;} return b;};",
        "exec fibi(10)",
    };

    std::vector<std::string> answer = {
        "parsed definition.\n",
        "55.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, invariant) {
    std::vector<std::string> target = {
        "def exp(x: double, n: double) {val a: double = x; var t: double = 1; for (i = 1, i < n) {t = t * a;} return t;};",
        "exec exp(10, 3)",
    };

    std::vector<std::string> answer = {
        "parsed definition.\n",
        "1000.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}
