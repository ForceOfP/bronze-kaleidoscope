/* #pragma once

#include <map>

#include "ast.hpp"
#include "lexer.hpp"

class Parser {
public:
    Token get_next_token();
    std::unique_ptr<ExprAST> parse_expression();
    std::unique_ptr<ExprAST> parse_number_expr();
    std::unique_ptr<ExprAST> parse_paren_expr();
    std::unique_ptr<ExprAST> parse_identifier_expr();
    std::unique_ptr<ExprAST> parse_primary();
    std::unique_ptr<ExprAST> parse_binary_operator_rhs();
    std::unique_ptr<ExprAST> parse_prototype();
    std::unique_ptr<ExprAST> parse_definition();
    std::unique_ptr<ExprAST> parse_top_level_expr();
    std::unique_ptr<ExprAST> parse_extern();
private:
    Token current_;
    std::map<std::string, int> operator_precedence_;

};

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
} */