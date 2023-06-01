#pragma once

#include <string>
#include <any>
#include <cassert>
#include <iostream>

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
    Init
};

class Token {
public:
    Token(TokenType t, std::string v): type_(t), value_(v) {}
    Token(TokenType t, double v): type_(t), value_(v) {}
    Token(TokenType t): type_(t) {}

    [[nodiscard]] std::string get_string() const;
    [[nodiscard]] double get_literal() const;

    friend std::ostream& operator<<(std::ostream& os, const Token& t) {
        switch (t.type_) {
        case TokenType::Eof:
            os << "[Eof] "; break;
        case TokenType::Def:
            os << "[Def] "; break;
        case TokenType::Extern:
            os << "[Extern] "; break;
        case TokenType::Delimiter:
            os << "[;] "; break;
        case TokenType::LeftParenthesis:
            os << "[(] "; break;
        case TokenType::RightParenthesis:
            os << "[)] "; break;
        case TokenType::Comma:
            os << "[,] "; break;
        case TokenType::Identifier:
            os << "[" << t.get_string() << "] "; break;
        case TokenType::Literal:
            os << "[" << t.get_literal() << "] "; break;
        case TokenType::Operator:
            os << "[" << t.get_string() << "] "; break;
        case TokenType::Init:
            break;
        }
        return os;
    }

    TokenType type_ = TokenType::Init;
    std::any value_;
};

