#pragma once

#include "ast.hpp"
#include "ast/type.hpp"
#include <unordered_map>
#include <vector>

namespace Semantic {

class NaiveSymbolTable {
public:
    NaiveSymbolTable() {
        table_ = {{}};
    }

    void add_symbol(std::string& name, TypeSystem::Type type);
    TypeSystem::Type find_symbol_type(std::string& name);
    void step();
    void back();
    void clear();

    using Segment = std::unordered_map<std::string, TypeSystem::Type>;
private:
    std::vector<Segment> table_;
};

}  // namespace Semantic

class TypeChecker {
public:
    bool check(ASTNode& a);
    bool check(const Body& body, TypeSystem::Type type);
    bool check(Expression* expr, TypeSystem::Type type);
    bool check(LiteralExpr* expr, TypeSystem::Type type);
    bool check(VariableExpr* expr, TypeSystem::Type type);
    bool check(BinaryExpr* expr, TypeSystem::Type type);
    bool check(CallExpr* expr, TypeSystem::Type type);
    bool check(IfExpr* expr, TypeSystem::Type type);
    bool check(ForExpr* expr, TypeSystem::Type type);
    bool check(UnaryExpr* expr, TypeSystem::Type type);
    bool check(VarDeclareExpr* expr, TypeSystem::Type type);
    bool check(ReturnExpr* expr, TypeSystem::Type type);

    // check against
    bool check_anonymous_expression(Expression* expr);
    void let_all_literal_typed(Expression* expr, TypeSystem::Type type);
    void let_all_literal_typed(CallExpr* expr);
public:
    std::string err_;
private:
    enum class AgainstStatus {Init, Checking, Checked};
    AgainstStatus anonymous_binary_status_;
    TypeSystem::Type anonymous_binary_type_;

    Semantic::NaiveSymbolTable symbol_table_;
    std::unordered_map<std::string, std::vector<TypeSystem::Type>> function_table_;
    TypeSystem::Type result_type_;
};

