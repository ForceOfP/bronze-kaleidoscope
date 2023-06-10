#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <variant>

#include "utils/rustic_match.hpp"

class Expression {
public:
    virtual ~Expression() = default;
};

struct LiteralExpr: public Expression {
    double value;
public:
    explicit LiteralExpr(double d): value(d) {}
};

struct VariableExpr: public Expression {
    std::string name;
public:
    explicit VariableExpr(std::string str): name(std::move(str)) {}
};

struct BinaryExpr: public Expression {
    std::string oper;
    std::unique_ptr<Expression> lhs;
    std::unique_ptr<Expression> rhs;
public:
    BinaryExpr(
        std::string op,
        std::unique_ptr<Expression> LHS,
        std::unique_ptr<Expression> RHS
    ): oper(std::move(op)), lhs(std::move(LHS)), rhs(std::move(RHS)) {}
};

struct CallExpr: public Expression {
    std::string callee;
    std::vector<std::unique_ptr<Expression>> args;

    CallExpr(std::string _callee, std::vector<std::unique_ptr<Expression>> _args):
        callee(std::move(_callee)), args(std::move(_args)) {}
};

/* struct Expression {
    std::variant<VariableExpr, BinaryExpr, CallExpr, LiteralExpr> data;
    explicit Expression(VariableExpr&& v): data(std::move(v)) {}
    explicit Expression(BinaryExpr&& b): data(std::move(b)) {}
    explicit Expression(CallExpr&& c): data(std::move(c)) {}
    explicit Expression(LiteralExpr&& l): data(l) {}

    template<typename VariableVisitor, typename BinaryVisitor, typename CallVisitor, typename LiteralVisitor>
    auto match(
        VariableVisitor variable_visitor, 
        BinaryVisitor binary_visitor,
        CallVisitor call_visitor,
        LiteralVisitor literal_visitor
    ) {
        return std::visit(overloaded{variable_visitor, binary_visitor, call_visitor, literal_visitor}, data);
    }
}; */

struct ProtoType {
    std::string name;
    std::vector<std::string> args;

    ProtoType(std::string _name, std::vector<std::string> _args):
        name(std::move(_name)), args(std::move(_args)) {}

    ProtoType(): name(""), args({}) {}
};

struct ExternNode {
    std::unique_ptr<ProtoType> prototype;
    
    explicit ExternNode(std::unique_ptr<ProtoType> _prototype): prototype(std::move(_prototype)) {}
};

struct FunctionNode {
    std::unique_ptr<ProtoType> prototype;
    std::unique_ptr<Expression> body;

    FunctionNode(std::unique_ptr<ProtoType> _prototype, std::unique_ptr<Expression> _body):
        prototype(std::move(_prototype)), body(std::move(_body)) {}
};

struct ASTNode {
    std::variant<ExternNode, FunctionNode> data;
    explicit ASTNode(ExternNode&& e): data(std::move(e)) {};
    explicit ASTNode(FunctionNode&& f): data(std::move(f)) {};

    template<typename ExternVisitor, typename FunctionVisitor>
    auto match(ExternVisitor extern_visitor, FunctionVisitor function_visitor) {
        return std::visit(overloaded{extern_visitor, function_visitor}, data);
    }
};

using ASTNodePtr = std::unique_ptr<ASTNode>;
using FunctionNodePtr = std::unique_ptr<FunctionNode>;
using ProtoTypePtr = std::unique_ptr<ProtoType>;
using ExpressionPtr = std::unique_ptr<Expression>;