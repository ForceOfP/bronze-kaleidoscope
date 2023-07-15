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
    Colon, // ':'
    Delimiter, //';'
    LeftParenthesis, // '('
    RightParenthesis, // ')'
    LeftSquareBrackets, // '['
    RightSquareBrackets, // ']'
    LeftCurlyBrackets, // '{'
    RightCurlyBrackets, // '}'
    Comma, // ','
    Identifier, // with a string
    Literal, // with a double
    Operator, // with a string
    If,
    Else,
    For,
    In,
    Binary,
    Unary,
    Var, // with a bool
    Return,
    Exec,
    Answer, // means ->, such as "def f() -> double {...}"  
    Struct,
    Dot, // '.'
    Init
};

class Token {
public:
    Token(TokenType t, std::string v): type_(t), value_(v) {}
    Token(TokenType t, double v): type_(t), value_(v) {}
    Token(TokenType t, bool c): type_(t), value_(c) {}
    explicit Token(TokenType t): type_(t) {}
    Token() = default;

    [[nodiscard]] std::string get_string() const;
    [[nodiscard]] double get_literal() const;
    [[nodiscard]] bool is_const() const;

    friend std::ostream& operator<<(std::ostream& os, const Token& t) {
        switch (t.type_) {
        case TokenType::Eof:
            os << "[Eof]"; break;
        case TokenType::Def:
            os << "[Def]"; break;
        case TokenType::Extern:
            os << "[Extern]"; break;
        case TokenType::Colon:
            os << "[:]"; break;
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
        case TokenType::For:
            os << "[For]"; break;
        case TokenType::In:
            os << "[In]"; break;
        case TokenType::Binary:
            os << "[Binary]"; break;
        case TokenType::Unary:
            os << "[Unary]"; break;
        case TokenType::Var:
            os << "[Var]"; break;
        case TokenType::LeftCurlyBrackets:
            os << "[{]"; break;
        case TokenType::RightCurlyBrackets:
            os << "[}]"; break;
        case TokenType::Return: 
            os << "[Return]"; break;
        case TokenType::Exec: 
            os << "[Exec]"; break;
        case TokenType::Answer: 
            os << "[->]"; break;
        case TokenType::LeftSquareBrackets: 
            os << "[ '[' ]"; break;
        case TokenType::RightSquareBrackets: 
            os << "[ ']' ]"; break;
        case TokenType::Struct:
            os << "[Struct]"; break;
        case TokenType::Dot:
            os << "[.]"; break;
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