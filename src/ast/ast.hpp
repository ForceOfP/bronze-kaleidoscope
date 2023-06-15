#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include <variant>

#include "utils/rustic_match.hpp"

class Expression {
public:
    virtual ~Expression() = default;
};

using ExpressionPtr = std::unique_ptr<Expression>;

struct LiteralExpr: public Expression {
    double value;
    explicit LiteralExpr(double d): value(d) {}
};

struct VariableExpr: public Expression {
    std::string name;
    explicit VariableExpr(std::string str): name(std::move(str)) {}
};

struct BinaryExpr: public Expression {
    std::string oper;
    std::unique_ptr<Expression> lhs;
    std::unique_ptr<Expression> rhs;
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

struct IfExpr: public Expression {
    std::unique_ptr<Expression> condition, then, _else;
    IfExpr(ExpressionPtr c, ExpressionPtr t, ExpressionPtr e): 
        condition(std::move(c)), then(std::move(t)), _else(std::move(e)) {}
};

struct ForExpr: public Expression {
    std::string var_name;
    ExpressionPtr start, end, step, body;
    ForExpr(std::string name, ExpressionPtr s, ExpressionPtr e, ExpressionPtr _step, ExpressionPtr b):
        var_name(std::move(name)), start(std::move(s)), end(std::move(e)), step(std::move(_step)), body(std::move(b)) {}
};

struct ProtoType {
    std::string name;
    std::vector<std::string> args;

    ProtoType(std::string _name, std::vector<std::string> _args):
        name(std::move(_name)), args(std::move(_args)) {}

    ProtoType(): name(""), args({}) {}

    friend std::ostream& operator<<(std::ostream& os, const ProtoType& t);
};

struct ExternNode {
    std::unique_ptr<ProtoType> prototype;
    
    explicit ExternNode(std::unique_ptr<ProtoType> _prototype): prototype(std::move(_prototype)) {}

    friend std::ostream& operator<<(std::ostream& os, const ExternNode& t);
};

struct FunctionNode {
    std::unique_ptr<ProtoType> prototype;
    std::unique_ptr<Expression> body;

    FunctionNode(std::unique_ptr<ProtoType> _prototype, std::unique_ptr<Expression> _body):
        prototype(std::move(_prototype)), body(std::move(_body)) {}

    friend std::ostream& operator<<(std::ostream& os, const FunctionNode& t);
};

struct ASTNode {
    std::variant<ExternNode, FunctionNode> data;
    explicit ASTNode(ExternNode&& e): data(std::move(e)) {};
    explicit ASTNode(FunctionNode&& f): data(std::move(f)) {};

    template<typename ExternVisitor, typename FunctionVisitor>
    auto match(ExternVisitor extern_visitor, FunctionVisitor function_visitor) {
        return std::visit(overloaded{extern_visitor, function_visitor}, data);
    }

    friend std::ostream& operator<<(std::ostream& os, ASTNode& t);
};

using ASTNodePtr = std::unique_ptr<ASTNode>;
using FunctionNodePtr = std::unique_ptr<FunctionNode>;
using ExternNodePtr = std::unique_ptr<ExternNode>;
using ProtoTypePtr = std::unique_ptr<ProtoType>;