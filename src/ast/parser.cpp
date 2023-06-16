#include "parser.hpp"
#include "ast/ast.hpp"
#include "ast/token.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
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

/// top_level_expression ::= expression
void Parser::parse_top_level_expression() {
    auto expression = parse_expression();
    if (!expression) {
        return;
    }

    // std::cout << "Parsed a top-level expr." << std::endl;
    
    auto proto = std::make_unique<ProtoType>("__anon_expr", std::vector<std::string>());
    ast_tree_.push_back(std::make_unique<ASTNode>(FunctionNode{std::move(proto), std::move(expression)}));
}

/// definition ::= 'def' prototype expression
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

    // std::cout << "Parsed a function definition." << std::endl;

    ast_tree_.push_back(std::make_unique<ASTNode>(FunctionNode{std::move(proto), std::move(expression)}));
}

/// external ::= 'extern' prototype
void Parser::parse_extern() {
    next_token(); // eat extern
    auto proto = parse_prototype();

    // std::cout << "Parsed an extern." << std::endl;

    ast_tree_.push_back(std::make_unique<ASTNode>(ExternNode{std::move(proto)}));
}

/// expression ::= primary binoprhs
ExpressionPtr Parser::parse_expression() {
    auto lhs = parse_unary();
    if (!lhs) {
        return nullptr;
    }

    return parse_binary_op_rhs(0, std::move(lhs));
}

/// prototype ::= id '(' id* ')' 
/// id in parenthesis are splited by ','
ProtoTypePtr Parser::parse_prototype() {
    std::string name = "";
    unsigned kind = 0; // 0 = ident, 1 = unary, 2 = binary
    int binary_precedence = 30;

    switch (current_token_type()) {
        case TokenType::Identifier:
            name = token_iter_->get_string();
            next_token();
            kind = 0;
            break;
        case TokenType::Binary:
            next_token(); // eat binary 
            if (current_token_type() != TokenType::Operator) {
                err_ = "Expected binary operator";
                return nullptr;
            }
            name = token_iter_->get_string();
            kind = 2;
            next_token(); // eat operator

            if (current_token_type() == TokenType::Literal) {
                double num = token_iter_->get_literal();
                if (num < 1 || num > 100) {
                    err_ = "Invalid precedence: must be 1..100";
                    return nullptr;
                }
                binary_precedence = static_cast<int>(num);
                operator_precedence_[name] = binary_precedence;
                next_token();
            }
            break;
        case TokenType::Unary:
            next_token(); // eat unary
            if (current_token_type() != TokenType::Operator) {
                err_ = "Expected binary operator";
                return nullptr;
            }

            name = token_iter_->get_string();
            kind = 1;
            operator_precedence_[name] = 10000; // max precedence
            next_token(); // eat operator
            break;
        default:
            err_ = "Expected function name in prototype";
            return nullptr;
    }

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

    if (kind && args.size() != kind) {
        err_ = "Invalid number of operands for operator";
        return nullptr;
    }

    return std::make_unique<ProtoType>(name, args, kind != 0, binary_precedence);
}

/// binoprhs::= [operator primary]*
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
        auto rhs = parse_unary();
        if (!rhs) {
            return nullptr;
        }

        // If BinOp binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS as its LHS.
        int next_prec = current_token_precedence();
        if (current_prec < next_prec) {
            rhs = parse_binary_op_rhs(current_prec + 1, std::move(rhs));
            if (!rhs) {
                return nullptr;
            }
        }

        lhs = std::make_unique<BinaryExpr>(oper, std::move(lhs), std::move(rhs));
    }
}

/// primary ::= identifierexpr | numberexpr | parenexpr
ExpressionPtr Parser::parse_primary() {
    switch (current_token_type()) {
    case TokenType::LeftParenthesis:
        return parse_parenthesis_expr();
    case TokenType::Identifier:
        return parse_identifier_expr();
    case TokenType::Literal:
        return parse_literal_expr();
    case TokenType::If:
        return parse_if_expr();
    case TokenType::For:
        return parse_for_expr();
    default:
        err_ = "unknown token when expecting an expression";
        return nullptr;
    }
}

/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
ExpressionPtr Parser::parse_for_expr() {
    next_token(); // eat for
    if (current_token_type() != TokenType::Identifier) {
        err_ = "expected identifier after for";
        return nullptr;
    }

    std::string variant_name = token_iter_->get_string();
    next_token(); // eat cycle variant

    if (current_token_type() != TokenType::Operator && token_iter_->get_string() != "=") {
        err_ = "expected '=' after identifier";
        return nullptr;
    }
    next_token(); // eat '='
    auto start = parse_expression();
    if (!start) {
        return nullptr;
    }

    if (current_token_type() != TokenType::Comma) {
        err_ = "expected ',' after for start value";
        return nullptr;
    }
    next_token(); // eat ','

    auto end = parse_expression();
    if (!end) {
        return nullptr;
    }

    // The step value is optional.
    ExpressionPtr step;
    if (current_token_type() == TokenType::Comma) {
        next_token(); // eat ','
        step = parse_expression();
        if (!step) {
            return nullptr;
        }
    }

    if (current_token_type() != TokenType::In) {
        err_ = "expected 'in' after for";
        return nullptr;
    }
    next_token(); // eat in

    auto body = parse_expression();
    if (!body) {
        return nullptr;
    }
    return std::make_unique<ForExpr>(
        variant_name,
        std::move(start),
        std::move(end),
        std::move(step),
        std::move(body)
    );
}

/// ifexpr ::= 'if' expression 'then' expression 'else' expression
ExpressionPtr Parser::parse_if_expr() {
    next_token(); // eat if

    //condition
    auto cond = parse_expression();
    if (!cond) return nullptr;

    if (current_token_type() != TokenType::Then) {
        err_ = "expected token 'then'";
        return nullptr;
    }
    next_token(); // eat then

    auto then = parse_expression();
    if (!then) return nullptr;

    if (current_token_type() != TokenType::Else) {
        err_ = "expected token 'else'";
        return nullptr;        
    }

    next_token(); // eat else
    auto _else = parse_expression();
    if (!_else) return nullptr;
    return std::make_unique<IfExpr>(std::move(cond), std::move(then), std::move(_else));
}

/// unary
///   ::= primary
///   ::= '!' unary
ExpressionPtr Parser::parse_unary() {
    // If the current token is not an operator, it must be a primary expr.
    if (current_token_type() == TokenType::LeftParenthesis
        || current_token_type() == TokenType::Comma
        || current_token_type() == TokenType::Identifier
        || current_token_type() == TokenType::Literal
        || current_token_type() == TokenType::For
        || current_token_type() == TokenType::If) {
        return parse_primary();
    }

    if (current_token_type() == TokenType::Operator) {
        std::string name = token_iter_->get_string();
        next_token(); // eat unary 
        if (auto opnd = parse_unary()) {
            return std::make_unique<UnaryExpr>(name, std::move(opnd));
        }
    }

    err_ = "parse unary unsuccess";
    return nullptr;
}

/// identifierexpr ::= identifier ['(' expression* ')']+
/// expression in parenthesis are splited by ','
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

/// literalexpr ::= literal
ExpressionPtr Parser::parse_literal_expr() {
    auto res = std::make_unique<LiteralExpr>((*token_iter_).get_literal());
    next_token();
    return res;
}

/// parenexpr ::= '(' expression ')'
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
