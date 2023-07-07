#include "semantic.hpp"
#include "ast/ast.hpp"
#include "ast/type.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace Semantic {

void NaiveSymbolTable::add_symbol(std::string& name, std::string& type) {
    table_.back().insert({name, type});
}

std::string& NaiveSymbolTable::find_symbol_type(std::string& name) {
    for (auto iter = table_.rbegin(); iter != table_.rend(); iter++) {
        if (iter->count(name)) {
            return (*iter)[name];
        }
    }

    return my_error_;
}

void NaiveSymbolTable::step() {
    table_.emplace_back();
}

void NaiveSymbolTable::back() {
    table_.pop_back();
}

void NaiveSymbolTable::clear() {
    table_ = {{}};
}

}  // namespace Semantic

bool TypeChecker::check(ASTNode& node) {
    symbol_table_.clear();
    return node.match(
        [&](ExternNode& e) -> bool {
            std::vector<std::string> types {};
            types.push_back(e.prototype->answer);
            for (auto& arg: e.prototype->args) {
                types.push_back(arg.second);
            }
            function_table_[e.prototype->name] = types;

            return true;
        }, 
        [&](FunctionNode& f) -> bool {
            if (f.prototype->name == "__anon_expr") {
                auto ret = f.body.data.front().get();
                auto ret_cast = dynamic_cast<ReturnExpr*>(ret);
                auto command = ret_cast->ret.get();
                if (f.prototype->answer != "uninit") {
                    if (check(command, f.prototype->answer)) {
                        let_all_literal_typed(command, f.prototype->answer);
                        return true;
                    } else {
                        return false;
                    }
                }

                anonymous_binary_type_str_ = "any";
                anonymous_binary_status_ = AgainstStatus::Init;
                if (!check_anonymous_expression(command)) {
                    err_ += "execution body type check error;";
                    return false;
                    if (!check_anonymous_expression(command)) {
                        err_ += "execution body type check error;";
                        return false;
                    }
                }
                if (anonymous_binary_type_str_ == "any") {
                    err_ += "cannot infer execution result type;";
                    return false;                    
                }
                f.prototype->answer = anonymous_binary_type_str_;
                let_all_literal_typed(command, f.prototype->answer);
                anonymous_binary_status_ = AgainstStatus::Init;
                return true;
            }
            std::vector<std::string> types {};
            types.push_back(f.prototype->answer);
            for (auto& arg: f.prototype->args) {
                types.push_back(arg.second);
                symbol_table_.add_symbol(arg.first, arg.second);
            }
            function_table_[f.prototype->name] = types;

            auto result_type = f.prototype->answer;
            result_type_ = result_type;
            return check(f.body, result_type);
        }
    );
}

bool TypeChecker::check(const Body& body, std::string& type) {
    auto n = body.data.size();
    auto limit = (body.has_return_value) ? n - 1 : n;
    for (int iter = 0; iter < limit; iter++) {
        auto expr = body.data[iter].get();
        bool valid = false;
        std::string my_any = "any";
        auto l = dynamic_cast<LiteralExpr*>(expr);
        if (l) {
            valid = check(l, my_any); 
        }

        auto c = dynamic_cast<CallExpr*>(expr);
        if (c) {
            valid = check(c, my_any);
        }
        
        auto v = dynamic_cast<VariableExpr*>(expr);
        if (v) {
            valid = check(v, my_any);
        }
        
        auto b = dynamic_cast<BinaryExpr*>(expr);
        if (b) {
            valid = check(b, my_any);
        }

        auto i = dynamic_cast<IfExpr*>(expr);
        if (i) {
            valid = check(i, my_any);
        }

        auto f = dynamic_cast<ForExpr*>(expr);
        if (f) {
            valid = check(f, my_any);
        }

        auto u = dynamic_cast<UnaryExpr*>(expr);
        if (u) {
            valid = check(u, my_any);
        }

        auto var = dynamic_cast<VarDeclareExpr*>(expr);
        if (var) {
            valid = check(var, my_any);
        }

        auto r = dynamic_cast<ReturnExpr*>(expr);
        if (r) {
            valid = check(r, result_type_);
        }

        if (!valid) {
            //err_ += "no expression find.";
            return false;
        }
    }
    if (body.has_return_value) {
        auto expr = body.data.back().get();
        bool valid = false;
        auto l = dynamic_cast<LiteralExpr*>(expr);
        if (l) {
            valid = check(l, type); 
        }

        auto c = dynamic_cast<CallExpr*>(expr);
        if (c) {
            valid = check(c, type);
        }
        
        auto v = dynamic_cast<VariableExpr*>(expr);
        if (v) {
            valid = check(v, type);
        }
        
        auto b = dynamic_cast<BinaryExpr*>(expr);
        if (b) {
            valid = check(b, type);
        }

        auto i = dynamic_cast<IfExpr*>(expr);
        if (i) {
            valid = check(i, type);
        }

        auto f = dynamic_cast<ForExpr*>(expr);
        if (f) {
            assert(false && "Shouldn't goto end-line for-expr\n");
        }

        auto u = dynamic_cast<UnaryExpr*>(expr);
        if (u) {
            valid = check(u, type);
        }

        auto var = dynamic_cast<VarDeclareExpr*>(expr);
        if (var) {
            static std::string tmp_void = "void";
            valid = TypeSystem::is_same_type(type, tmp_void);
        }

        auto r = dynamic_cast<ReturnExpr*>(expr);
        if (r) {
            valid = check(r, result_type_);
        }

        if (!valid) {
            //err_ += "body-return type check error";
            return false;
        }
    }
    return true;
}

bool TypeChecker::check(Expression* expr, std::string& type) {
    auto l = dynamic_cast<LiteralExpr*>(expr);
    if (l) {
        return check(l, type);
    }

    auto c = dynamic_cast<CallExpr*>(expr);
    if (c) {
        return check(c, type);
    }
    
    auto v = dynamic_cast<VariableExpr*>(expr);
    if (v) {
        return check(v, type);
    }
    
    auto b = dynamic_cast<BinaryExpr*>(expr);
    if (b) {
        return check(b, type);
    }

    auto i = dynamic_cast<IfExpr*>(expr);
    if (i) {
        return check(i, type);
    }

    auto f = dynamic_cast<ForExpr*>(expr);
    if (f) {
        return check(f, type);
    }

    auto u = dynamic_cast<UnaryExpr*>(expr);
    if (u) {
        return check(u, type);
    }

    auto var = dynamic_cast<VarDeclareExpr*>(expr);
    if (var) {
        return check(var, type);
    }

    auto r = dynamic_cast<ReturnExpr*>(expr);
    if (r) {
        return check(r, result_type_);
    }

    err_ += "no expression find;";
    return false;
}

bool TypeChecker::check(LiteralExpr* expr, std::string& type) {
    if (anonymous_binary_status_ == AgainstStatus::Checked) {
        expr->type = anonymous_binary_type_str_;
        // std::cout << "Literal " << expr->value << " type is " << TypeSystem::get_type_str(expr->type) << std::endl;
        return true;
    }
    
    if (expr->type == "uninit") {
        expr->type = type;
        // std::cout << "Literal " << expr->value << " type is " << TypeSystem::get_type_str(expr->type) << std::endl;
        return true; 
    } else {
        if (!TypeSystem::is_same_type(expr->type, type)) {
            err_ += "Literal check error;";
            err_ += "found " + expr->type;
            err_ += " expect " + type + ';';
            return false;
        } else {
            // std::cout << "Literal " << expr->value << " type is " << TypeSystem::get_type_str(expr->type) << std::endl;
            return true;
        }
    }
}

bool TypeChecker::check(VariableExpr* expr, std::string& type) {
    auto _type = symbol_table_.find_symbol_type(expr->name);
    if (TypeSystem::is_same_type(_type, type)) {
        if (anonymous_binary_status_ == AgainstStatus::Checking) {
            if (anonymous_binary_type_str_ == "any") {
                anonymous_binary_type_str_ = _type;
            }
        }
        return true;
    } else {
        err_ += "variable type check error;";
        err_ += "found " + symbol_table_.find_symbol_type(expr->name);
        err_ += " expect " + type + ';';

        return false;
    }
}

bool TypeChecker::check(BinaryExpr* expr, std::string& type) {
    if (expr->oper == "=") {
        auto lhs = expr->lhs.get();
        auto variable = static_cast<VariableExpr*>(lhs);
        if (!variable) {
            err_ += "lhs should be variable;";
            return false;
        } else {
            type = symbol_table_.find_symbol_type(variable->name);
        }

        if (!expr->rhs) {
            err_ += "no rhs;";
            return false;
        }
        if (check(expr->rhs.get(), type)) {
            let_all_literal_typed(expr->rhs.get(), type);
            return true;
        }
        return false;
    }

    if (!(expr->lhs && check(expr->lhs.get(), type))) {
        err_ += "lhs type check error;";
        return false;
    }

    if (!(expr->rhs && check(expr->rhs.get(), type))) {
        err_ += "rhs type check error;";
        return false;
    }

    return true;
}

bool TypeChecker::check(CallExpr* expr, std::string& type) {
    auto& target_function = function_table_[expr->callee];
    if (target_function.size() - 1 != expr->args.size()) {
        err_ += "function args size not match, expect " + std::to_string(target_function.size() - 1) + " but " + std::to_string(expr->args.size()) + ';';
        return false;
    }
    auto iter = target_function.begin();
    if (!TypeSystem::is_same_type(*iter, type)) {
        err_ += "function return type check error;";
        return false;
    }
    
    if (anonymous_binary_status_ == AgainstStatus::Checking) {
        if (anonymous_binary_type_str_ == "any") {
            anonymous_binary_type_str_ = *iter;
        }
    }

    iter++;
    for (auto& arg: expr->args) {
        if (!check(arg.get(), *iter)) {
            iter++;
            err_ += "function arg type check error;";
            return false;
        }
    }

    let_all_literal_typed(expr);
    return true;
}

bool TypeChecker::check(IfExpr* expr, std::string& type) {
    if (!check(expr->then, type)) {
        err_ += "then body type check error;";
        return false;        
    }

    if (!check(expr->_else, type)) {
        err_ += "else body type check error;";
        return false; 
    }

/*     if (!check(expr->condition.get(), TypeSystem::Type::Any)) {
        err_ += "condition type check error;";
        return false;
    } */
    anonymous_binary_type_str_ = "any";
    anonymous_binary_status_ = AgainstStatus::Init;
    
    if (!check_anonymous_expression(expr->condition.get())) {
        err_ += "condition body type check error;";
        return false;
    } else {
        if (!check_anonymous_expression(expr->condition.get())) {
            err_ += "condition body type check-against error;";
            return false;
        } 
    }
    anonymous_binary_status_ = AgainstStatus::Init;

    return true;
}

bool TypeChecker::check(ForExpr* expr, std::string& type) {
    if (!check(expr->body, type)) {
        err_ += "loop body type check error;";
        return false;        
    }
    return true;
}

bool TypeChecker::check(UnaryExpr* expr, std::string& type) {
    auto& args = function_table_[expr->_operater];
    if (!TypeSystem::is_same_type(args[0], type)) {
        err_ += "unary return type check error;";
        return false;        
    }

    if (!TypeSystem::is_same_type(args[1], type)) {
        err_ += "unary operand type check error;";
        return false;        
    }

    return true;
}

bool TypeChecker::check(VarDeclareExpr* expr, std::string& type) {
    symbol_table_.add_symbol(expr->name, expr->type);
    let_all_literal_typed(expr->value.get(), expr->type);
    return true;
}

bool TypeChecker::check(ReturnExpr* expr, std::string& type) {
    if (!check(expr->ret.get(), type)) {
        err_ += "return type check error;";
        return false;        
    }
    let_all_literal_typed(expr->ret.get(), type);
    return true;
}

bool TypeChecker::check_anonymous_expression(Expression* expr) {
    switch (anonymous_binary_status_) {
    case AgainstStatus::Init:
        anonymous_binary_status_ = AgainstStatus::Checking;
        break;
    case AgainstStatus::Checking:
        anonymous_binary_status_ = AgainstStatus::Checked;
        break;
    case AgainstStatus::Checked:
        break;
    }
    static std::string my_any = "any";

    auto l = dynamic_cast<LiteralExpr*>(expr);
    if (l) {
        return true;
    }

    auto c = dynamic_cast<CallExpr*>(expr);
    if (c) {
        return check(c, my_any);
    }
    
    auto v = dynamic_cast<VariableExpr*>(expr);
    if (v) {
        return true;
    }
    
    auto b = dynamic_cast<BinaryExpr*>(expr);
    if (b) {
        return check(b, my_any);
    }

    auto u = dynamic_cast<UnaryExpr*>(expr);
    if (u) {
        return check(u, my_any);
    }

    err_ = "anonymous expression check error;";
    return false;
}

void TypeChecker::let_all_literal_typed(Expression* expr, std::string& type) {
    auto l = dynamic_cast<LiteralExpr*>(expr);
    if (l) {
        l->type = type;
        return;
    }
        
    auto b = dynamic_cast<BinaryExpr*>(expr);
    if (b) {
        let_all_literal_typed(b->lhs.get(), type);
        let_all_literal_typed(b->rhs.get(), type);
    }

    auto u = dynamic_cast<UnaryExpr*>(expr);
    if (u) {
        let_all_literal_typed(u->operand.get(), type);
    }

    auto c = dynamic_cast<CallExpr*>(expr);
    if (c) {
        let_all_literal_typed(c);
    }

    return;    
}

void TypeChecker::let_all_literal_typed(CallExpr* expr) {
    auto& args_type = function_table_[expr->callee];
    int iter = 1;
    for (auto& arg: expr->args) {
        let_all_literal_typed(arg.get(), args_type[iter]);
        iter++;
    }
}