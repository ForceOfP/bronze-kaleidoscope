#pragma once

#include <cassert>
#include <gtest/gtest.h>
#include <iostream>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <vector>

#include "ast/ast.hpp"
#include "ast/lexer.hpp"
#include "ast/parser.hpp"
#include "ast/semantic.hpp"
#include "codegen/jit_codegen.hpp"

void codegen_helper(std::vector<std::string>& target, std::vector<std::string>& answer) {
    std::string ans = "";
    llvm::raw_string_ostream output(ans); 
    CodeGeneratorSetting setting = {
        .print_ir = false,
        .function_pass_optimize = true,
    };
    auto generator = JitCodeGenerator(output, setting);
    TypeChecker checker(generator.type_manager_);

    for (int i = 0; i < target.size(); i++) {
        auto parser = Parser(Lexer::tokenize(target[i]), generator.binary_oper_precedence_);
        auto asts = parser.parse();

        for (auto& ast: asts) {
            if (!checker.check(*ast)) {
                std::cout << "TypeChecker failed at line " << i << std::endl;
                std::cout << checker.err_ << std::endl;
                assert(false);
            }
        }
        generator.codegen(std::move(asts));
        ASSERT_EQ(ans, answer[i]);
        ans.clear();
    }
}

TEST(CODEGEN, single) {
    std::vector<std::string> target = {
        "def f(x: double) -> double { return x + x; }",
        "extern sin(x: double) -> double",
        "exec: double 2 + 3"
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "declare double @sin(double)\n",
        "5.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, optimize) {
    std::vector<std::string> target = {
        "def g(x: double)  -> double { return 2 + 3 + x; }", // TODO(lbt): "x + 2 + 3" will break the constant folding, check what happened.
        "def h(x: double, y: double)  -> double { return x * y + y * x; }",
        "def i(x: double)  -> double {return (1 + 2 + x)*(x + (1 + 2));};",
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "parsed function definition.\n",
        "parsed function definition.\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, jit) {
    std::vector<std::string> target = {
        "def f(x: double, y: double, z: double) -> double {return x * y + z;}",
        "exec f(4, 2, 3)",
        "exec f(1, 2, 3)"
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "11.000000\n",
        "5.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);    
}

TEST(CODEGEN, ext) {
    std::vector<std::string> target = {
        "extern sin(x: double) -> double",
        "extern cos(x: double) -> double",
        "def f(x: double) -> double {return sin(x) * sin(x) + cos(x) * cos(x);}",
        "exec f(1.0)",
        "exec f(4.0)"
    };

    std::vector<std::string> answer = {
        "declare double @sin(double)\n",
        "declare double @cos(double)\n",
        // TODO(lbt): there are still some folding problem...
        "parsed function definition.\n",
        "1.000000\n",
        "1.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, ifelse1) {
    std::vector<std::string> target = {
        "def double(x: double) -> double {return x + x;}",
        "def triple(x: double) -> double {return x * 3;}",
        // "def f(x: double) -> double {if (x < 3: double) {return double(x);} else {return triple(x);}};",
        "def f(x: double) -> double {if (x < 3) {return double(x);} else {return triple(x);}};",
        "exec f(2)",
        "exec f(5)"
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "parsed function definition.\n",
        "parsed function definition.\n",
        "4.000000\n",
        "15.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, ifelse2) {
    std::vector<std::string> target = {
        "def double(x: double) -> double {return x + x;}",
        "def triple(x: double) -> double {return x * 3;}",
        // "def f(x: double) -> double {return if (x < 3: double) {double(x)} else {triple(x)}};",
        "def f(x: double) -> double {return if (x < 3) {double(x)} else {triple(x)}};",
        "exec f(2)",
        "exec f(5)"
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "parsed function definition.\n",
        "parsed function definition.\n",
        "4.000000\n",
        "15.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, loop) {
    std::vector<std::string> target = {
        "def sum(n: double) -> double {var a: double = 0; var b: double = 0; for (i = 0: double, i < n) {b = a + b; a = a + 1;} return b; }",
        "exec sum(100)",
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "5050.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, oper) {
    std::vector<std::string> target = {
/*         "def unary - (v) 0 - v;",
        "-(2+3)", */
        "def unary ! (v: double) -> double {if (v) {return 0;} else {return 1;}};",
        "exec: double !1",
        "exec: double !0",
        "def binary | 5 (LHS: double, RHS: double) -> double {if (LHS) {return 1;} else {if (RHS) {return 1;} else {return 0;}}};",
        "exec: double 0|0",
        "exec: double 1|0",
        "def binary & 6 (LHS: double, RHS: double) -> double {if (!LHS) {return 0;} else {return !(!RHS);}};",
        "exec: double 1&0",
        "exec: double 1&1",
        "def binary > 10 (LHS: double, RHS: double) -> double {return RHS < LHS;};",
        "exec: double 1 > 2",
        "exec: double 2 > 1",
        "def binary == 9 (LHS: double, RHS: double) -> double {return !(RHS < LHS | RHS > LHS);};",
        "exec: double 1 == 0",
        "exec: double 1 == 1",
    };

    std::vector<std::string> answer = {
/*         "parsed function definition.\n",
        "-5.000000\n", */
        "parsed function definition.\n",
        "0.000000\n",
        "1.000000\n",
        "parsed function definition.\n",
        "0.000000\n",
        "1.000000\n",
        "parsed function definition.\n",
        "0.000000\n",
        "1.000000\n",
        "parsed function definition.\n",
        "0.000000\n",
        "1.000000\n",
        "parsed function definition.\n",
        "0.000000\n",
        "1.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, variant) {
    std::vector<std::string> target = {
        "def fibi(x: double) -> double {var a: double = 1; var b: double = 1; var c: double; for (i = 3: double, i < x) {c = a + b; a = b; b = c;} return b;};",
        "exec fibi(10)",
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "55.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, invariant) {
    std::vector<std::string> target = {
        "def exp(x: double, n: double) -> double {val a: double = x; var t: double = 1; for (i = 1: double, i < n) {t = t * a;} return t;};",
        "exec exp(10, 3)",
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "1000.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, loopInt) {
    std::vector<std::string> target = {
        "def sum(n: double) -> i32 {var a: i32 = 0; var b: i32 = 0; for (i = 0: double, i < n) {b = a + b; a = a + 1;} return b; }",
        "exec sum(100)",
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "5050\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, addInt) {
    std::vector<std::string> target = {
        "def myadd(n: i32) -> i32 {return n+1;}",
        "exec myadd(100)",
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "101\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, arrayLoad) {
    std::vector<std::string> target = {
        "def f() -> double {var x:array\%double\%2 = [2, 3]:double; return x[1];}",
        "exec f()",
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "3.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, arrayStore) {
    std::vector<std::string> target = {
        "def f() -> double {var x:array\%double\%2 = [2, 3]:double; x[1] = 4; return x[1] + x[0];}",
        "exec f()",
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "6.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, arrayFibo) {
    std::vector<std::string> target = {
        "def f(n: double) -> double {var x:array\%double\%2 = [1, 1]:double; for (i = 1: double, i < n - 2:double) {var tmp:double = x[1];x[1] = x[0] + x[1];x[0] = tmp;}; return x[1];}",
        "exec f(10)",
        "exec f(9)",
        "exec f(11)",
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "55.000000\n",
        "34.000000\n",
        "89.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, arrayNested) {
    std::vector<std::string> target = {
        "def f() -> double {var x:array\%array\%double\%2\%2 = [[2, 2]:double, [3, 3]:double]:double; x[0][1] = 4; return x[0][1] + x[1][0];}",
        "exec f()",
    };

    std::vector<std::string> answer = {
        "parsed function definition.\n",
        "7.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, structSimple) {
    std::vector<std::string> target = {
        "struct Foo {a: double, b: double,}",
        "def f() -> double {var x: Foo; x.a = 1.0; return x.a;}",
        "exec f()"
    };

    std::vector<std::string> answer = {
        "parsed struct definition.\n",
        "parsed function definition.\n",
        "1.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, structWithDifferentTypes) {
    std::vector<std::string> target = {
        "struct Foo {a: i32, b: double,}",
        "def f() -> i32 {var x: Foo; x.a = 1; return x.a;}",
        "def g() -> double {var x: Foo; x.b = 2.0; return x.b;}",
        "exec f()",
        "exec g()"
    };

    std::vector<std::string> answer = {
        "parsed struct definition.\n",
        "parsed function definition.\n",
        "parsed function definition.\n",
        "1\n",
        "2.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, structContainsArray) {
    std::vector<std::string> target = {
        "struct Foo {a: double, b: array\%double\%2,}",
        "def f() -> double {var x: Foo; x.b = [1.0, 2.0]:double; return x.b[1];}",
        "exec f()"
    };

    std::vector<std::string> answer = {
        "parsed struct definition.\n",
        "parsed function definition.\n",
        "2.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

TEST(CODEGEN, structNested) {
    std::vector<std::string> target = {
        "struct Foo {m: double, n: i32,}",
        "struct Bar {a: double, b: array\%Foo\%2,}",
        "def f() -> double {var x: Bar; x.b[1].m = 1.0; return x.b[1].m;}",
        "exec f()"
    };

    std::vector<std::string> answer = {
        "parsed struct definition.\n",
        "parsed struct definition.\n",
        "parsed function definition.\n",
        "1.000000\n",
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}
