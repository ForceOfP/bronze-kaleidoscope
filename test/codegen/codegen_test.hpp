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
        "define double @f(double \%x) {\n"
        "entry:\n"
        "  \%addtmp = fadd double \%x, \%x\n"
        "  ret double \%addtmp\n"
        "}\n",
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
        "define double @g(double \%x) {\n"
        "entry:\n"
        "  \%addtmp = fadd double \%x, 5.000000e+00\n"
        "  ret double \%addtmp\n"
        "}\n",
        "define double @h(double \%x, double \%y) {\n"
        "entry:\n"
        "  \%multmp = fmul double \%x, \%y\n"
        "  \%addtmp = fadd double \%multmp, \%multmp\n"
        "  ret double \%addtmp\n"
        "}\n",
        "define double @i(double \%x) {\n"
        "entry:\n"
        "  \%addtmp = fadd double \%x, 3.000000e+00\n"
        "  \%multmp = fmul double \%addtmp, \%addtmp\n"
        "  ret double \%multmp\n"
        "}\n"
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
        "define double @f(double \%x, double \%y, double \%z) {\n"
        "entry:\n"
        "  \%multmp = fmul double \%x, \%y\n"
        "  \%addtmp = fadd double \%multmp, \%z\n"
        "  ret double \%addtmp\n"
        "}\n",
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
        "define double @f(double \%x) {\n" 
        "entry:\n"
        "  \%calltmp = call double @sin(double \%x)\n"
        "  \%calltmp1 = call double @sin(double \%x)\n"
        "  \%multmp = fmul double \%calltmp, \%calltmp1\n"
        "  \%calltmp2 = call double @cos(double \%x)\n"
        "  \%calltmp3 = call double @cos(double \%x)\n"
        "  \%multmp4 = fmul double \%calltmp2, \%calltmp3\n"
        "  \%addtmp = fadd double \%multmp, \%multmp4\n"
        "  ret double \%addtmp\n"
        "}\n",
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
        "define double @double(double \%x) {\n"
        "entry:\n"
        "  \%addtmp = fadd double \%x, \%x\n"
        "  ret double \%addtmp\n"
        "}\n",
        "define double @triple(double \%x) {\n"
        "entry:\n"
        "  \%multmp = fmul double \%x, 3.000000e+00\n"
        "  ret double \%multmp\n"
        "}\n",
        "define double @f(double \%x) {\n"
        "entry:\n"
        "  \%0 = fcmp ult double \%x, 3.000000e+00\n"
        "  br i1 \%0, label \%then, label \%else\n"
        "\n"
        "then:                                             ; preds = \%entry\n"
        "  \%calltmp = call double @double(double \%x)\n"
        "  br label \%ifcont\n"
        "\n"
        "else:                                             ; preds = \%entry\n"
        "  \%calltmp1 = call double @triple(double \%x)\n"
        "  br label \%ifcont\n"
        "\n"
        "ifcont:                                           ; preds = \%else, \%then\n"
        "  \%iftmp = phi double [ \%calltmp, \%then ], [ \%calltmp1, \%else ]\n"
        "  ret double \%iftmp\n"
        "}\n",
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
        "define double @printstar(double \%n) {\n"
        "entry:\n"
        "  br label \%loop\n"
        "\n"
        "loop:                                             ; preds = \%loop, \%entry\n"
        "  \%i = phi double [ 0.000000e+00, \%entry ], [ \%nextvar, \%loop ]\n"
        "  \%calltmp = call double @putchard(double 4.200000e+01)\n"
        "  \%nextvar = fadd double \%i, 1.000000e+00\n"
        "  \%0 = fcmp ult double \%i, \%n\n"
        "  br i1 \%0, label \%loop, label \%afterloop\n"
        "\n"
        "afterloop:                                        ; preds = \%loop\n"
        "  ret double 0.000000e+00\n"
        "}\n",
        // TODO(lbt): this example should be updated.
        "0.000000\n",
        "0.000000\n"
    };

    assert(target.size() == answer.size());
    codegen_helper(target, answer);
}

