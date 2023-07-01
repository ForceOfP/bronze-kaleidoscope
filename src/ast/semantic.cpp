#include "semantic.hpp"
#include "ast/ast.hpp"
#include "ast/type.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

namespace Semantic {

void NaiveSymbolTable::add_symbol(std::string& name, TypeSystem::Type type) {
    table_.back().insert({name, type});
}

TypeSystem::Type NaiveSymbolTable::find_symbol_type(std::string& name) {
    for (auto iter = table_.rbegin(); iter != table_.rend(); iter++) {
        if (iter->count(name)) {
            return (*iter)[name];
        }
    }

    return TypeSystem::Type::Error;
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
            std::vector<TypeSystem::Type> types {};
            types.push_back(e.prototype->answer);
            for (auto& arg: e.prototype->args) {
                types.push_back(arg.second);
            }
            function_table_[e.prototype->name] = types;

            return true;
        }, 
        [&](FunctionNode& f) -> bool {
            if (f.prototype->name == "__anon_expr") {
                return true;
            }
            std::vector<TypeSystem::Type> types {};
            types.push_back(f.prototype->answer);
            for (auto& arg: f.prototype->args) {
                types.push_back(arg.second);
                symbol_table_.add_symbol(arg.first, arg.second);
            }
            function_table_[f.prototype->name] = types;

            auto return_type = f.prototype->answer;
            return_type_ = return_type;
            return check(f.body, return_type);
        }
    );
}

bool TypeChecker::check(const Body& body, TypeSystem::Type type) {
    auto n = body.data.size();
    auto limit = (body.has_return_value) ? n - 1 : n;
    for (int iter = 0; iter < limit; iter++) {
        auto expr = body.data[iter].get();
        bool valid = false;
        auto l = dynamic_cast<LiteralExpr*>(expr);
        if (l) {
            valid = check(l, TypeSystem::Type::Any); 
        }

        auto c = dynamic_cast<CallExpr*>(expr);
        if (c) {
            valid = check(c, TypeSystem::Type::Any);
        }
        
        auto v = dynamic_cast<VariableExpr*>(expr);
        if (v) {
            valid = check(v, TypeSystem::Type::Any);
        }
        
        auto b = dynamic_cast<BinaryExpr*>(expr);
        if (b) {
            valid = check(b, TypeSystem::Type::Any);
        }

        auto i = dynamic_cast<IfExpr*>(expr);
        if (i) {
            valid = check(i, TypeSystem::Type::Any);
        }

        auto f = dynamic_cast<ForExpr*>(expr);
        if (f) {
            valid = check(f, TypeSystem::Type::Any);
        }

        auto u = dynamic_cast<UnaryExpr*>(expr);
        if (u) {
            valid = check(u, TypeSystem::Type::Any);
        }

        auto var = dynamic_cast<VarDeclareExpr*>(expr);
        if (var) {
            valid = check(var, TypeSystem::Type::Any);
        }

        auto r = dynamic_cast<ReturnExpr*>(expr);
        if (r) {
            valid = check(r, return_type_);
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
            valid = TypeSystem::is_same_type(TypeSystem::Type::Void, type);
        }

        auto r = dynamic_cast<ReturnExpr*>(expr);
        if (r) {
            valid = check(r, return_type_);
        }

        if (!valid) {
            //err_ += "body-return type check error";
            return false;
        }
    }
    return true;
}

bool TypeChecker::check(Expression* expr, TypeSystem::Type type) {
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
        return check(r, return_type_);
    }

    err_ += "no expression find;";
    return false;
}

bool TypeChecker::check(LiteralExpr* expr, TypeSystem::Type type) {
    if (expr->type == TypeSystem::Type::Uninit) {
        expr->type = type;
        // std::cout << "Literal " << expr->value << " type is " << TypeSystem::get_type_str(expr->type) << std::endl;
        return true; 
    } else {
        if (!TypeSystem::is_same_type(expr->type, type)) {
            err_ += "Literal check error;";
            err_ += "found " + TypeSystem::get_type_str(expr->type);
            err_ += " expect " + TypeSystem::get_type_str(type) + ';';
            return false;
        } else {
            // std::cout << "Literal " << expr->value << " type is " << TypeSystem::get_type_str(expr->type) << std::endl;
            return true;
        }
    }
}

bool TypeChecker::check(VariableExpr* expr, TypeSystem::Type type) {
    if (TypeSystem::is_same_type(symbol_table_.find_symbol_type(expr->name), type)) {
        return true;
    } else {
        err_ += "variable type check error;";
        err_ += "found " + TypeSystem::get_type_str(symbol_table_.find_symbol_type(expr->name));
        err_ += " expect " + TypeSystem::get_type_str(type) + ';';

        return false;
    }
}

bool TypeChecker::check(BinaryExpr* expr, TypeSystem::Type type) {
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
        return check(expr->rhs.get(), type);
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

bool TypeChecker::check(CallExpr* expr, TypeSystem::Type type) {
    auto& target_function = function_table_[expr->callee];
    if (target_function.size() - 1 != expr->args.size()) {
        err_ += "function args size no match;";
        return false;
    }
    auto iter = target_function.begin();
    if (*iter != type) {
        err_ += "function return type check error;";
        return false;
    }
    
    iter++;
    for (auto& arg: expr->args) {
        if (!check(arg.get(), *iter)) {
            iter++;
            err_ += "function arg type check error;";
            return false;
        }
    }
    return true;
}

bool TypeChecker::check(IfExpr* expr, TypeSystem::Type type) {
    if (!check(expr->then, type)) {
        err_ += "then body type check error;";
        return false;        
    }

    if (!check(expr->_else, type)) {
        err_ += "else body type check error;";
        return false; 
    }

    if (!check(expr->condition.get(), TypeSystem::Type::Any)) {
        err_ += "condition type check error;";
        return false;
    }

    return true;
}

bool TypeChecker::check(ForExpr* expr, TypeSystem::Type type) {
    return true;
}

bool TypeChecker::check(UnaryExpr* expr, TypeSystem::Type type) {
    auto& args = function_table_[expr->_operater];
    if (!TypeSystem::is_same_type(type, args[0])) {
        err_ += "unary return type check error;";
        return false;        
    }

    if (!TypeSystem::is_same_type(type, args[1])) {
        err_ += "unary operand type check error;";
        return false;        
    }

    return true;
}

bool TypeChecker::check(VarDeclareExpr* expr, TypeSystem::Type type) {
    symbol_table_.add_symbol(expr->name, expr->type);
    return true;
}

bool TypeChecker::check(ReturnExpr* expr, TypeSystem::Type type) {
    if (!check(expr->ret.get(), type)) {
        err_ += "return type check error;";
        return false;        
    }
    return true;
}