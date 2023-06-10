#include "parser.hpp"
#include "ast/ast.hpp"
#include "ast/token.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

std::vector<ASTNodePtr> Parser::parse() {
    bool end = false;
    while (!end) {
        switch (current_token_type()) {
        case TokenType::Eof:
            end = true;
            break;
        case TokenType::Def:
            parse_definition();
            break;
        case TokenType::Extern:
            parse_extern();
            break;
        case TokenType::Delimiter:
            next_token();
            break;
        default:
            parse_top_level_expression();
            break;
        }
        
        if (!err_.empty()) {
            std::cerr << err_ << std::endl;
            return {};
        }
    }

    return std::move(ast_tree_);
}
 
void Parser::parse_top_level_expression() {
    auto expression = parse_expression();
    if (!expression) {
        return;
    }

    std::cout << "Parsed a top-level expr." << std::endl;
    
    auto proto = std::make_unique<ProtoType>("__anon_expr", std::vector<std::string>());
    auto function = FunctionNode{std::move(proto), std::move(expression)};
    ast_tree_.push_back(std::make_unique<ASTNode>(FunctionNode{std::move(proto), std::move(expression)}));
}

void Parser::parse_definition() {
    next_token(); //eat def
    auto proto = parse_prototype();
    if (!proto) {
        return;
    }

    auto expression = parse_expression();
    if (!expression) {
        return;
    }

    std::cout << "Parsed a function definition." << std::endl;

    ast_tree_.push_back(std::make_unique<ASTNode>(FunctionNode{std::move(proto), std::move(expression)}));
}

void Parser::parse_extern() {
    next_token(); // eat extern
    auto proto = parse_prototype();

    std::cout << "Parsed an extern." << std::endl;

    ast_tree_.push_back(std::make_unique<ASTNode>(ExternNode{std::move(proto)}));
}

ExpressionPtr Parser::parse_expression() {
    auto lhs = parse_primary();
    if (!lhs) {
        return nullptr;
    }

    return parse_binary_op_rhs(0, std::move(lhs));
}

ProtoTypePtr Parser::parse_prototype() {
    if (current_token_type() != TokenType::Identifier) {
        err_ = "Expected function name in prototype";
        return nullptr;
    }

    auto name = (*token_iter_).get_string();
    next_token();

    if (current_token_type() != TokenType::LeftParenthesis) {
        err_ = "Expected '(' in prototype";
        return nullptr;        
    }

    next_token();
    std::vector<std::string> args{};
    while (current_token_type() != TokenType::RightParenthesis) {
        if (current_token_type() == TokenType::Identifier) {
            args.push_back((*token_iter_).get_string());
            next_token();
            if (current_token_type() == TokenType::Comma) {
                next_token(); // eat ','
                if (current_token_type() != TokenType::Identifier) {
                    err_ = "Expected identifier before ','";
                    return nullptr;
                }
            } else {
                if (current_token_type() != TokenType::RightParenthesis) {
                    err_ = "Expected ')' in prototype";
                    return nullptr;
                }                
            }
        } else {
            err_ = "Expected identifier before '('";
            return nullptr;            
        }
    }
    next_token(); // eat ')'

    return std::make_unique<ProtoType>(name, args);
}

ExpressionPtr Parser::parse_binary_op_rhs(int prec, ExpressionPtr lhs) {
    for (;;) {
        int current_prec = current_token_precedence();
        if (current_prec < 0) {
            return lhs;
        }

        // If this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done.
        if (current_prec < prec) {
            return lhs;
        }

        // Okay, we know this is a binop.
        std::string oper = (*token_iter_).get_string();
        next_token();

        // Parse the primary expression after the binary operator.
        auto rhs = parse_primary();
        if (!rhs) {
            return nullptr;
        }

        // If BinOp binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS as its LHS.
        int next_prec = current_token_precedence();
        if (prec < next_prec) {
            rhs = parse_binary_op_rhs(prec + 1, std::move(rhs));
            if (!rhs) {
                return nullptr;
            }
        }

        lhs = std::make_unique<BinaryExpr>(oper, std::move(lhs), std::move(rhs));
    }
}

ExpressionPtr Parser::parse_primary() {
    switch (current_token_type()) {
    case TokenType::LeftParenthesis:
        return parse_parenthesis_expr();
    case TokenType::Identifier:
        return parse_identifier_expr();
    case TokenType::Literal:
        return parse_literal_expr();
    default:
        err_ = "unknown token when expecting an expression";
        return nullptr;
    }
}

ExpressionPtr Parser::parse_identifier_expr() {
    std::string name = (*token_iter_).get_string();
    next_token(); // eat name

    // variable
    if (current_token_type() != TokenType::LeftParenthesis) {
        return std::make_unique<VariableExpr>(name); 
    }

    // call
    next_token(); // eat '('
    std::vector<ExpressionPtr> args{};
    if (current_token_type() != TokenType::RightParenthesis) {
        while (true) {
            if (auto arg = parse_expression()) {
                args.push_back(std::move(arg));
            } else {
                return nullptr;
            }

            if (current_token_type() == TokenType::RightParenthesis) {
                break;
            }

            if (current_token_type() != TokenType::Comma) {
                err_ = "Expected ')' or ',' in argument list";
                return nullptr;
            }
            next_token();
        }
    }
    next_token(); // eat ')'

    return std::make_unique<CallExpr>(name, std::move(args));
}

ExpressionPtr Parser::parse_literal_expr() {
    auto res = std::make_unique<LiteralExpr>((*token_iter_).get_literal());
    next_token();
    return res;
}

ExpressionPtr Parser::parse_parenthesis_expr() {
    next_token(); // eat '('
    auto res = parse_expression();
    if (!res) {
        return nullptr;
    }

    if (current_token_type() != TokenType::RightParenthesis) {
        err_ = "expected ')'";
        return nullptr;
    }
    next_token(); // eat ')'
    return res;
}

int Parser::current_token_precedence() {
    if (token_iter_ == tokens_.end()) return -1;

    if (current_token_type() == TokenType::Operator) {
        if (operator_precedence_.count((*token_iter_).get_string())) {
            return operator_precedence_[(*token_iter_).get_string()];
        } else {
            return -1;
        }
    } else {
        return -1;
    }
}
