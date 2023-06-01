/* #include "parser.hpp"

Token Parser::get_next_token() {
    //return current_ = Lexer::get_token();
}

/// numberexpr ::= number
std::unique_ptr<ExprAST> Parser::parse_number_expr() {
    auto result = std::make_unique<NumberExprAST>(current_.get_literal());
    get_next_token(); // consume the number
    return std::move(result);
}

 */