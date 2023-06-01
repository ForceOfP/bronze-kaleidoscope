#include "token.hpp"

std::string Token::get_string() const {
    assert(this->type_ == TokenType::Identifier || this->type_ == TokenType::Operator);

    return std::any_cast<std::string>(this->value_);
}

double Token::get_literal() const {
    assert(this->type_ == TokenType::Literal);

    return std::any_cast<double>(this->value_);
}