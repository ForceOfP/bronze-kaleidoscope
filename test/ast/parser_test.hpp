#pragma once

#include <gtest/gtest.h>
#include "ast/ast.hpp"
#include "ast/lexer.hpp"
#include "ast/parser.hpp"

std::string serialize_asts(std::vector<ASTNodePtr>& asts) {
    std::stringstream ss;

    for (const auto& ast: asts) {
        ss << *ast << ' ';
    }

    std::string ans = ss.str();
    ans.pop_back();
    return ans;
}

TEST(AST, parse) {
    std::vector<std::string> target = {
        "def f(x) x + x",
        "2.3",
        "extern sin(x)",
    };

    std::vector<std::string> answer = {
        "[Function]: \n\t[Name]: f\n\t[Args]: x ",
        "[Function]: \n\t[Name]: __anon_expr\n",
        "[Extern]: \n\t[Name]: sin\n\t[Args]: x "
    };

    assert(target.size() == answer.size());
    for (int i = 0; i < target.size(); i++) {
        auto parser = Parser(Lexer::tokenize(target[i]));
        auto ans = parser.parse();

        ASSERT_EQ(serialize_asts(ans), answer[i]);
    }
}