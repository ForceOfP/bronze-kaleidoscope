#pragma once

#include <gtest/gtest.h>
#include "ast/ast.hpp"
#include "ast/lexer.hpp"
#include "ast/parser.hpp"

std::map<std::string, int> parser_prec = {{"<", 10}, {"+", 20}, {"-", 20}, {"*", 40}};

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
    const std::vector<std::string> target = {
        "def f(x) { return x + x; }",
        "2.3",
        "extern sin(x)",
        "if (1) {2} else {3};",
        "for (i = 0, i < n, 1.0) {1};",
        "for (i = 0, i < n) {1};",
        "def g(x) { return 1; }"
    };

    const std::vector<std::string> answer = {
        "[Function]: \n\t[Name]: f\n\t[Args]: x \n\t[Body]: \n\t\t[Return]\n",
        "[Function]: \n\t[Name]: __anon_expr\n\n\t[Body]: \n\t\t[Literal]\n",
        "[Extern]: \n\t[Name]: sin\n\t[Args]: x ",
        "[Function]: \n\t[Name]: __anon_expr\n\n\t[Body]: \n\t\t[If]\n",
        "[Function]: \n\t[Name]: __anon_expr\n\n\t[Body]: \n\t\t[For]\n",
        "[Function]: \n\t[Name]: __anon_expr\n\n\t[Body]: \n\t\t[For]\n",
        "[Function]: \n\t[Name]: g\n\t[Args]: x \n\t[Body]: \n\t\t[Return]\n",
    };

    assert(target.size() == answer.size());
    for (int i = 0; i < target.size(); i++) {
        auto parser = Parser(Lexer::tokenize(target[i]), parser_prec);
        auto ans = parser.parse();

        ASSERT_EQ(serialize_asts(ans), answer[i]);
    }
}