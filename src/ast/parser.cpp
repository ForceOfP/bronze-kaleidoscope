#include "parser.hpp"
#include "ast/ast.hpp"
#include "ast/token.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
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
        case TokenType::Struct:
            parse_struct_definition();
            break;
        case TokenType::Delimiter:
            next_token();
            break;
        case TokenType::Exec:
            parse_top_level_expression();
            break;            
        default:
            std::cerr << "cannot parse input" << std::endl;
            return {};
        }
        
        if (!err_.empty()) {
            std::cerr << err_ << std::endl;
            return {};
        }
    }

    return std::move(ast_tree_);
}

void Parser::parse_struct_definition() {
    next_token(); // eat 'struct'

    if (current_token_type() != TokenType::Identifier) {
        err_ = "expect struct-name before struct";
        return;
    }
    std::string name = token_iter_->get_string(); 
    next_token(); // eat struct name

    if (current_token_type() != TokenType::LeftCurlyBrackets) {
        err_ = "expect { before 'struct'";
        return;
    }

    next_token(); // eat '{'

    std::vector<std::pair<std::string, std::string>> elements {};
    while (current_token_type() != TokenType::RightCurlyBrackets) {
        if (current_token_type() != TokenType::Identifier) {
            err_ = "expect struct-element-name in struct content";
            return;
        }
        std::string element_name = token_iter_->get_string(); 
        next_token();
        if (current_token_type() != TokenType::Colon) {
            err_ = "expect struct ':' before struct-element-name";
            return;
        }
        next_token(); // eat ':'
        if (current_token_type() != TokenType::Identifier) {
            err_ = "expect struct-element-type before ':'";
            return;
        }
        std::string element_type = token_iter_->get_string(); 
        next_token();        

        if (current_token_type() != TokenType::Comma) {
            err_ = "expect ',' at the last of struct line";
            return;
        }
        next_token(); // eat ','        
        elements.emplace_back(element_name, element_type);
    }

    if (current_token_type() != TokenType::RightCurlyBrackets) {
        err_ = "expect } before struct-content";
        return;
    }

    next_token(); // eat '}'
    ast_tree_.push_back(std::make_unique<ASTNode>(StructNode{name, elements}));
}

/// top_level_expression ::= expression
void Parser::parse_top_level_expression() {
    next_token(); // eat 'exec'

    // std::unique_ptr<TypeSystem::TypeBase> result_type = std::make_unique<TypeSystem::UninitType>();
    std::string result_type = "uninit";

    if (current_token_type() == TokenType::Colon) {
        next_token(); // eat ':'
        if (current_token_type() != TokenType::Identifier) {
            err_ = "expect type-id before 'exec:'";
            return;
        }
        result_type = token_iter_->get_string();
        next_token();
    }

    auto expression = parse_expression();
    if (!expression) {
        return;
    }

    Body body = {};
    auto ret = std::make_unique<ReturnExpr>(std::move(expression));
    body.data.push_back(std::move(ret));
    body.has_return_value = false;

    // std::cout << "Parsed a top-level expr." << std::endl;
    
    auto proto = std::make_unique<ProtoType>("__anon_expr");
    proto->answer = std::move(result_type); 
    ast_tree_.push_back(std::make_unique<ASTNode>(FunctionNode{std::move(proto), std::move(body)}));
}

/// definition ::= 'def' prototype { body }
void Parser::parse_definition() {
    next_token(); //eat def
    auto proto = parse_prototype();
    if (!proto) {
        return;
    }

    if (current_token_type() != TokenType::LeftCurlyBrackets) {
        err_ = "expected '{' before prototype";
        return;
    }
    next_token(); // eat {

/*     auto expression = parse_expression();
    if (!expression) {
        return;
    } */
    auto body = parse_body();
    if (!err_.empty()) {
        return;
    }

    if (current_token_type() != TokenType::RightCurlyBrackets) {
        err_ = "expected '}' before prototype";
        return;
    }
    next_token(); // eat }
    // std::cout << "Parsed a function definition." << std::endl;

    //ast_tree_.push_back(std::make_unique<ASTNode>(FunctionNode{std::move(proto), std::move(expression)}));
    ast_tree_.push_back(std::make_unique<ASTNode>(FunctionNode{std::move(proto), std::move(body)}));
}

/// external ::= 'extern' prototype
void Parser::parse_extern() {
    next_token(); // eat extern
    auto proto = parse_prototype();

    // std::cout << "Parsed an extern." << std::endl;

    ast_tree_.push_back(std::make_unique<ASTNode>(ExternNode{std::move(proto)}));
}

/// body ::= (expression;)* return_expression;
Body Parser::parse_body() {
    Body body;

    bool returned = false;
    while (current_token_type() != TokenType::RightCurlyBrackets) {
        auto line = parse_expression();
        if (!line) {
            return {};
        }
        if (current_token_type() == TokenType::Delimiter) {
            if (returned) {
                err_ = "Expect non-delimiter at body end.";
                return {};
            }
            next_token(); // eat ';'
            returned = false;
        } else if (prev_token_is_right_curly_brackets()) {
            if (returned) {
                err_ = "Expect non-delimiter at body end.";
                return {};
            }
            returned = false;            
        }else {
            if (returned) {
                err_ = "Expect one non-delimiter at body.";
                return {};
            }
            returned = true;
        }
        body.data.push_back(std::move(line));
    }
    body.has_return_value = returned;
    return body;
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
    std::vector<std::pair<std::string, std::string>> args{};
    while (current_token_type() != TokenType::RightParenthesis) {
        if (current_token_type() == TokenType::Identifier) {
            args.emplace_back((*token_iter_).get_string(), "error");
            next_token();
            if (current_token_type() != TokenType::Colon) {
                err_ = "Expected ':' after identifier";
                return nullptr;
            }
            next_token(); // eat ':'
            args.back().second = token_iter_->get_string();
            next_token(); // eat type

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

    std::string return_type = "void";
    if (current_token_type() == TokenType::Answer) {
        next_token(); // eat '->'
        if (current_token_type() != TokenType::Identifier) {
            err_ = "Expected type before '->'";
            return nullptr;
        }
        return_type = token_iter_->get_string();
        next_token(); // eat type
    }

    if (kind && args.size() != kind) {
        err_ = "Invalid number of operands for operator";
        return nullptr;
    }

    return std::make_unique<ProtoType>(name, std::move(args), kind != 0, binary_precedence, std::move(return_type));
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
    case TokenType::Var:
        return parse_var_declare_expr();
    case TokenType::Return:
        return parse_return_expr();
    case TokenType::LeftSquareBrackets:
        return parse_square_array_expr();
    default:
        err_ = "unknown token when expecting an expression";
        return nullptr;
    }
}

/// varexpr ::= ['var'|'val'] identifier ':' type ('=' expression)?
ExpressionPtr Parser::parse_var_declare_expr() {
    bool is_const = token_iter_->is_const();
    next_token(); // eat var

    if (current_token_type() != TokenType::Identifier) {
        err_ = "expected identifier after var";
        return nullptr;
    }

    std::string name = token_iter_->get_string();
    next_token(); // eat identifier

    if (current_token_type() != TokenType::Colon) {
        err_ = "expected ':' after identifier";
        return nullptr;
    }
    next_token(); // eat ':'

    if (current_token_type() != TokenType::Identifier) {
        err_ = "expected type after colon";
        return nullptr;
    }
    std::string type = token_iter_->get_string();
    next_token(); // eat type

    ExpressionPtr init = nullptr;
    if (current_token_type() == TokenType::Operator && token_iter_->get_string() == "=") {
        next_token(); // eat the '=';

        init = parse_primary();
        if (!init) return nullptr;
    }

    // names.emplace_back(name, std::move(init));

    return std::make_unique<VarDeclareExpr>(std::move(type), name, std::move(init), is_const);
}

/// forexpr ::= 'for' '(' identifier '=' expr ',' expr (',' expr)? ')' { body }
ExpressionPtr Parser::parse_for_expr() {
    next_token(); // eat for

    if (current_token_type() != TokenType::LeftParenthesis) {
        err_ = "expected token '(' before if";
        return nullptr;
    }
    next_token(); // eat '('

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

    if (current_token_type() != TokenType::RightParenthesis) {
        err_ = "expected token ')' before loop-condition";
        return nullptr;
    }
    next_token(); // eat ')'

    if (current_token_type() != TokenType::LeftCurlyBrackets) {
        err_ = "expected '{' after for-body";
        return nullptr;
    }
    next_token(); // eat '{'

    auto body = parse_body();
    if (!err_.empty()) {
        return nullptr;
    }

    if (current_token_type() != TokenType::RightCurlyBrackets) {
        err_ = "expected '}' before for-body";
        return nullptr;
    }
    next_token(); // eat '}'

    return std::make_unique<ForExpr>(
        variant_name,
        std::move(start),
        std::move(end),
        std::move(step),
        std::move(body)
    );
}

/// ifexpr ::= 'if' '(' expression ')' '{' body '}' ('else' '{' body '}')?
ExpressionPtr Parser::parse_if_expr() {
    next_token(); // eat if

    if (current_token_type() != TokenType::LeftParenthesis) {
        err_ = "expected token '(' before if";
        return nullptr;
    }
    next_token(); // eat '('

    //condition
    auto cond = parse_expression();
    if (!cond) return nullptr;

    if (current_token_type() != TokenType::RightParenthesis) {
        err_ = "expected token ')'";
        return nullptr;
    }
    next_token(); // eat ')'

    if (current_token_type() != TokenType::LeftCurlyBrackets) {
        err_ = "expected token '{'";
        return nullptr;
    }
    next_token(); // eat '{'

    auto then = parse_body();
    if (!err_.empty()) return nullptr;

    if (current_token_type() != TokenType::RightCurlyBrackets) {
        err_ = "expected token '}'";
        return nullptr;
    }
    next_token(); // eat '}'

    if (current_token_type() != TokenType::Else) {
        return std::make_unique<IfExpr>(std::move(cond), std::move(then));
    }
    next_token(); // eat else

    if (current_token_type() != TokenType::LeftCurlyBrackets) {
        err_ = "expected token '{'";
        return nullptr;
    }
    next_token(); // eat '{'
    
    auto _else = parse_body();
    if (!err_.empty()) return nullptr;
    
    if (current_token_type() != TokenType::RightCurlyBrackets) {
        err_ = "expected token '}'";
        return nullptr;
    }
    next_token(); // eat '}'

    return std::make_unique<IfExpr>(std::move(cond), std::move(then), std::move(_else));
}

/// return ::= 'return' expression;
ExpressionPtr Parser::parse_return_expr() {
    next_token(); // eat return
    auto ret = parse_expression();
    return std::make_unique<ReturnExpr>(std::move(ret));
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
        || current_token_type() == TokenType::If
        || current_token_type() == TokenType::Var
        || current_token_type() == TokenType::Return
        || current_token_type() == TokenType::LeftSquareBrackets) {
        return parse_primary();
/*         auto opnd = parse_primary();

        return opnd; */
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

/// identifierexpr ::= identifier '(' expression* ')'
///                ::= identifier
///                ::= identifier [ '[' expression ']' ]*
///                ::= identifier [ '.' expression]*
/// expression in parenthesis are splited by ','
ExpressionPtr Parser::parse_identifier_expr() {
    std::string name = (*token_iter_).get_string();
    next_token(); // eat name

    // variable
    if (current_token_type() != TokenType::LeftParenthesis
        && current_token_type() != TokenType::LeftSquareBrackets
        && current_token_type() != TokenType::Dot) {
        return std::make_unique<VariableExpr>(name); 
    }

    if (current_token_type() == TokenType::LeftSquareBrackets || current_token_type() == TokenType::Dot) {
        // offset address
        std::vector<ExpressionPtr> offset_indexes {};
        std::vector<VariableExpr::Addr> addrs {};

        bool all_array = true;
        while (current_token_type() == TokenType::LeftSquareBrackets
                || current_token_type() == TokenType::Dot) {
            if (current_token_type() == TokenType::Dot) {
                all_array = false;
                next_token(); // eat '.'
                if (current_token_type() != TokenType::Identifier) {
                    err_ = "expect identifier before '.'";
                    return nullptr;
                } 
                addrs.emplace_back(token_iter_->get_string());
                next_token(); // eat Identifier             
            } else {
                next_token(); // eat '['
                auto index = parse_unary();
                if (!index) {
                    err_ = "need value in []";
                    return nullptr;
                }
                addrs.emplace_back(std::move(index));
                if (current_token_type() == TokenType::RightSquareBrackets) {
                    next_token(); // eat ']'
                } else {
                    err_ = "expect ']' before '['";
                    return nullptr;
                }                
            }
        }
        return std::make_unique<VariableExpr>(name, std::move(addrs), all_array); 
    }

    if (current_token_type() == TokenType::LeftParenthesis) {
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
    return nullptr;
}

/// literalexpr ::= literal
ExpressionPtr Parser::parse_literal_expr() {
    double val = (*token_iter_).get_literal();
    next_token();
    std::string type = "uninit";
    if (current_token_type() == TokenType::Colon) {
        next_token(); // eat ':'

        if (current_token_type() != TokenType::Identifier) {
            err_ = "expected type after colon";
            return nullptr;
        }
        type = token_iter_->get_string();
        next_token(); // eat type        
    }
    auto res = std::make_unique<LiteralExpr>(val, std::move(type));
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

/// square_array_expr ::= '[' expression [',' expression]* ']' ':' type
ExpressionPtr Parser::parse_square_array_expr() {
    std::vector<ExpressionPtr> elements;
    
    next_token(); // eat '[';
    auto element = parse_unary();
    if (!element) {
        return nullptr;
    }
    elements.push_back(std::move(element));

    while (current_token_type() == TokenType::Comma) {
        next_token(); // eat ','
        auto _element = parse_unary();
        if (!_element) {
            return nullptr;
        }
        elements.push_back(std::move(_element));            
    }

    if (current_token_type() != TokenType::RightSquareBrackets) {
        err_ = "expected ']' before '['";
        return nullptr;
    }
    next_token(); // eat ']'

    if (current_token_type() != TokenType::Colon) {
        err_ = "expected ':' before ']'";
        return nullptr;
    }
    next_token(); // eat ':'

    if (current_token_type() != TokenType::Identifier) {
        err_ = "expected type before ':'";
        return nullptr;
    }
    std::string type = token_iter_->get_string();
    type = "array%" + type + '%' + std::to_string(elements.size());
    next_token();
    return std::make_unique<ArrayExpr>(std::move(elements), std::move(type));
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

bool Parser::prev_token_is_right_curly_brackets() {
    token_iter_--;
    if (current_token_type() == TokenType::RightCurlyBrackets) {
        token_iter_++;
        return true;
    }
    token_iter_++;
    return false;
}