#include <gtest/gtest.h>
#include "ast/lexer.hpp"

std::string serialize_tokens (std::vector<Token>& tokens) {
    std::stringstream ss;

    for (const auto& token: tokens) {
        ss << token << ' ';
    }

    std::string ans = ss.str();
    ans.pop_back();
    return ans;
}

TEST(AST, tokenize) {
    std::vector<std::string> target = {
        "def f(x) x + x",
        "2.3",
        "abc",
        "2",
        ";,()+-*=",
        "extern",
        "def f(x) x + x ###afeabgnan####",
    };
    std::vector<std::string> answer = {
        "[Def] [Ident: f] [(] [Ident: x] [)] [Ident: x] [Oper: +] [Ident: x]",
        "[Literal: 2.3]",
        "[Ident: abc]",
        "[Literal: 2]",
        "[;] [,] [(] [)] [Oper: +] [Oper: -] [Oper: *] [Oper: =]",
        "[Extern]",
        "[Def] [Ident: f] [(] [Ident: x] [)] [Ident: x] [Oper: +] [Ident: x]",
    };

    assert(target.size() == answer.size());
    for (int i = 0; i < target.size(); i++) {
        auto ans = Lexer::tokenize(target[i]);

        ASSERT_EQ(serialize_tokens(ans), answer[i]);
    }
}