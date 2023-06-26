#include "lexer.hpp"
#include "ast/token.hpp"
#include <iostream>

#include <re2/re2.h>

std::vector<Token> Lexer::tokenize(std::string target_) {
    static const re2::RE2 comment_regex("(?m)#.*");
    RE2::GlobalReplace(&target_, comment_regex, "");
    //std::cout << target << std::endl;
    
    static const re2::RE2 token_regex(
        "(?P<ident>[[:alpha:]]\\w*)|"
        "(?P<number>\\d+\\.?\\d*)|"
        "(?P<delimiter>;)|"
        "(?P<oppar>\\()|"
        "(?P<clpar>\\))|"
        "(?P<opcur>{)|"
        "(?P<clcur>})|"
        "(?P<comma>,)|"
        "(?P<operator>[!-'*-/:<-@\\^`|~][!-'*-/:<-@\\^`|~]?)"
    );

    std::vector<re2::StringPiece> matched_tokens(token_regex.NumberOfCapturingGroups() + 1);
    re2::StringPiece target = target_;
    std::vector<Token> ans {};
    size_t start = 0;

    while (start < target.size()) {
        while (target[start] == ' ' || target[start] == '\t' || target[start] == '\n') {
            start++;
        }   
        if (start == target.size()) break;

        if (!token_regex.Match(target, start, target.size(), RE2::ANCHOR_START,
                                matched_tokens.data(), matched_tokens.size())) {
            std::cerr << "Failed to lex input at dis: " << start << std::endl;
            //break;
            return {};
        } 

        for (const auto& token_group: token_regex.NamedCapturingGroups()) {
            re2::StringPiece token_context = matched_tokens[token_group.second];

            if (!token_context.empty()) {
                start += token_context.size();
                auto token_tag = token_group.first;
                if (token_tag == "ident") {
                    if (token_context == "def") {
                        ans.emplace_back(TokenType::Def);
                    } else if (token_context == "extern") {
                        ans.emplace_back(TokenType::Extern);
                    } else if (token_context == "if") {
                        ans.emplace_back(TokenType::If);
                    } else if (token_context == "else") {
                        ans.emplace_back(TokenType::Else);
                    } else if (token_context == "for") {
                        ans.emplace_back(TokenType::For);
                    } else if (token_context == "in") {
                        ans.emplace_back(TokenType::In);
                    } else if (token_context == "binary") {
                        ans.emplace_back(TokenType::Binary);
                    } else if (token_context == "unary") {
                        ans.emplace_back(TokenType::Unary);
                    } else if (token_context == "var") {
                        ans.emplace_back(TokenType::Var, false);
                    }  else if (token_context == "val") {
                        ans.emplace_back(TokenType::Var, true);
                    } else if (token_context == "return") {
                        ans.emplace_back(TokenType::Return);
                    } else {
                        ans.emplace_back(TokenType::Identifier, std::string(token_context));
                    }
                } else if (token_tag == "number") {
                    double num = std::stod(std::string(token_context));
                    ans.emplace_back(TokenType::Literal, num);
                } else if (token_tag == "delimiter") {
                    ans.emplace_back(TokenType::Delimiter);
                } else if (token_tag == "oppar") {
                    ans.emplace_back(TokenType::LeftParenthesis);
                } else if (token_tag == "clpar") {
                    ans.emplace_back(TokenType::RightParenthesis);
                } else if (token_tag == "opcur") {
                    ans.emplace_back(TokenType::LeftCurlyBrackets);
                } else if (token_tag == "clcur") {
                    ans.emplace_back(TokenType::RightCurlyBrackets);
                } else if (token_tag == "comma") {
                    ans.emplace_back(TokenType::Comma);
                } else if (token_tag == "operator") {
                    ans.emplace_back(TokenType::Operator, std::string(token_context));
                } else {
                    std::cerr << "Lexer parse error!" << '\n';
                    exit(1);
                }
            }
        }
    }

    ans.emplace_back(TokenType::Eof);
    
    return ans;
}