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

    void add_symbol(std::string& name, std::string& type);
    std::string& find_symbol_type(std::string& name);
    void step();
    void back();
    void clear();

    using Segment = std::unordered_map<std::string, std::string>;
private:
    std::vector<Segment> table_;
    std::string my_error_ = "error";
};

}  // namespace Semantic

class TypeChecker {
public:
    TypeChecker(std::unordered_map<std::string, TypeSystem::AggregateType>& struct_table, TypeManager& type_manager)
        : struct_table_(struct_table), type_manager_(type_manager) {}

    bool check(ASTNode& a);
    bool check(const Body& body, std::string& type);
    bool check(Expression* expr, std::string& type);
    bool check(LiteralExpr* expr, std::string& type);
    bool check(VariableExpr* expr, std::string& type);
    bool check(BinaryExpr* expr, std::string& type);
    bool check(CallExpr* expr, std::string& type);
    bool check(IfExpr* expr, std::string& type);
    bool check(ForExpr* expr, std::string& type);
    bool check(UnaryExpr* expr, std::string& type);
    bool check(VarDeclareExpr* expr, std::string& type);
    bool check(ReturnExpr* expr, std::string& type);
    bool check(ArrayExpr* expr, std::string& type);

    // check against
    bool check_anonymous_expression(Expression* expr);
    void let_all_literal_typed(Expression* expr, std::string& type);
    void let_all_literal_typed(CallExpr* expr);
public:
    std::string err_;
private:
    enum class AgainstStatus {Init, Checking, Checked};
    AgainstStatus anonymous_binary_status_;
    std::string anonymous_binary_type_str_;

    Semantic::NaiveSymbolTable symbol_table_;
    std::unordered_map<std::string, std::vector<std::string>> function_table_;
    std::string result_type_;

    std::string my_int32 = "i32";

    std::unordered_map<std::string, TypeSystem::AggregateType>& struct_table_;
    TypeManager& type_manager_;
};

