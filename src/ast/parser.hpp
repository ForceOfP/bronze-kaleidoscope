#pragma once

#include <algorithm>
#include <cassert>
#include <iterator>
#include <map>
#include <exception>
#include <optional>
#include <functional>
#include <string>
#include <vector>

#include "ast.hpp"
#include "token.hpp"

class Parser {
public:
    explicit Parser(std::vector<Token>&& v, std::map<std::string, int>& prec)
        : tokens_(std::move(v)), operator_precedence_(prec) {
        token_iter_ = tokens_.begin();
    }

    std::vector<ASTNodePtr> parse();
    void parse_top_level_expression();
    void parse_definition();
    void parse_extern();
    void parse_struct_definition();

    Body parse_body();
    ExpressionPtr parse_expression();
    ProtoTypePtr parse_prototype();

    ExpressionPtr parse_binary_op_rhs(int prec, ExpressionPtr lhs);
    ExpressionPtr parse_primary();

    ExpressionPtr parse_return_expr();
    ExpressionPtr parse_var_declare_expr();
    ExpressionPtr parse_unary();
    ExpressionPtr parse_if_expr();
    ExpressionPtr parse_for_expr();
    ExpressionPtr parse_identifier_expr();
    ExpressionPtr parse_literal_expr();
    ExpressionPtr parse_parenthesis_expr();
    ExpressionPtr parse_square_array_expr();
private:
    TokenType current_token_type() { return token_iter_->type_; }
    void next_token() { token_iter_++; }
    int current_token_precedence();
    bool prev_token_is_right_curly_brackets();
private:
    using TokenVecIter = std::vector<Token>::iterator;
    std::vector<Token> tokens_;
    std::vector<ASTNodePtr> ast_tree_;
    TokenVecIter token_iter_;
    std::string err_;
    std::map<std::string, int>& operator_precedence_;
};
