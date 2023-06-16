#pragma once

#include <string>
#include <any>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

enum class TokenType {
    Eof,
    Def, 
    Extern, 
    Delimiter, //';'
    LeftParenthesis, // '('
    RightParenthesis, // ')'
    Comma, // ','
    Identifier, // with a string
    Literal, // with a double
    Operator, // with a string
    If,
    Else,
    Then,
    For,
    In,
    Binary, // with a string
    Unary, // with a string
    Init
};

class Token {
public:
    Token(TokenType t, std::string v): type_(t), value_(v) {}
    Token(TokenType t, double v): type_(t), value_(v) {}
    explicit Token(TokenType t): type_(t) {}
    Token() = default;

    [[nodiscard]] std::string get_string() const;
    [[nodiscard]] double get_literal() const;

    friend std::ostream& operator<<(std::ostream& os, const Token& t) {
        switch (t.type_) {
        case TokenType::Eof:
            os << "[Eof]"; break;
        case TokenType::Def:
            os << "[Def]"; break;
        case TokenType::Extern:
            os << "[Extern]"; break;
        case TokenType::Delimiter:
            os << "[;]"; break;
        case TokenType::LeftParenthesis:
            os << "[(]"; break;
        case TokenType::RightParenthesis:
            os << "[)]"; break;
        case TokenType::Comma:
            os << "[,]"; break;
        case TokenType::Identifier:
            os << "[Ident: " << t.get_string() << "]"; break;
        case TokenType::Literal:
            os << "[Literal: " << t.get_literal() << "]"; break;
        case TokenType::Operator:
            os << "[Oper: " << t.get_string() << "]"; break;
        case TokenType::Init:
            break;
        case TokenType::If:
            os << "[If]"; break;
        case TokenType::Else:
            os << "[Else]"; break;
        case TokenType::Then:
            os << "[Then]"; break;
        case TokenType::For:
            os << "[For]"; break;
        case TokenType::In:
            os << "[In]"; break;
        case TokenType::Binary:
            os << "[Binary]"; break;
        case TokenType::Unary:
            os << "[Unary]"; break;
        default:
            break;
        }
        return os;
    }

    TokenType type_ = TokenType::Init;
    std::any value_;
};

using TokenPtr = std::unique_ptr<Token>;
using TokenPtrVec = std::vector<TokenPtr>;