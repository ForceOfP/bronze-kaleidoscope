#pragma once

#include <vector>

#include "token.hpp"

class Lexer {
public:
    static std::vector<Token> tokenize(std::string target);
};