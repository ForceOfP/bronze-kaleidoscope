#pragma once

#include <cassert>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include <variant>

#include "utils/rustic_match.hpp"
#include "type.hpp"

class Expression {
public:
    virtual ~Expression() = default;
};

using ExpressionPtr = std::unique_ptr<Expression>;

struct ArrayExpr: public Expression {
    std::vector<ExpressionPtr> elements;
    std::string type;
    //TypeSystem::TypeEnum type;
    ArrayExpr(std::vector<ExpressionPtr> _elements, std::string _type): elements(std::move(_elements)), type(std::move(_type)) {}
};

struct Body {
    std::vector<ExpressionPtr> data;
    bool has_return_value;
    Body(std::vector<ExpressionPtr> body, bool _has_return_value): 
        data(std::move(body)), has_return_value(_has_return_value) {}
    Body() = default;
};

// using Body = std::vector<ExpressionPtr>;

struct LiteralExpr: public Expression {
    double value;
    std::string type;

    explicit LiteralExpr(double d, std::string _type): value(d), type(std::move(_type)) {}
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
    std::unique_ptr<Expression> condition;
    Body then, _else;
    IfExpr(ExpressionPtr c, Body t, Body e = {}): 
        condition(std::move(c)), then(std::move(t)), _else(std::move(e)) {}
};

struct ForExpr: public Expression {
    std::string var_name;
    ExpressionPtr start, end, step;
    Body body;
    ForExpr(std::string name, ExpressionPtr s, ExpressionPtr e, ExpressionPtr _step, Body b):
        var_name(std::move(name)), start(std::move(s)), end(std::move(e)), step(std::move(_step)), body(std::move(b)) {}
};

struct UnaryExpr: public Expression {
    std::string _operater;
    ExpressionPtr operand;
    std::vector<ExpressionPtr> indexes;

    UnaryExpr(std::string oper, ExpressionPtr opnd):
        _operater(std::move(oper)), operand(std::move(opnd)) {}
};

struct VarDeclareExpr: public Expression {    
    //TypeSystem::TypeEnum type;
    std::string type;
    std::string name;
    ExpressionPtr value;
    bool is_const;

    explicit VarDeclareExpr(
        std::string _type, std::string _name, ExpressionPtr expr, bool _is_const):
        type(std::move(_type)), name(std::move(_name)), value(std::move(expr)), is_const(_is_const) {}
};

struct ReturnExpr: public Expression {
    ExpressionPtr ret;

    explicit ReturnExpr(ExpressionPtr _ret): ret(std::move(_ret)) {}  
};

struct ProtoType {
    std::string name;
    std::vector<std::pair<std::string, std::string>> args;
    std::string answer;
    bool is_operator_;
    unsigned precedence_;

    explicit ProtoType(std::string _name, std::vector<std::pair<std::string, std::string>> _args = {},
                bool is_oper = false, unsigned prec = 0,
                std::string answer_t = "uninit"):
        name(std::move(_name)), 
        args(std::move(_args)), 
        answer(std::move(answer_t)), 
        is_operator_(is_oper),
        precedence_(prec) {}

    ProtoType(): name(""), args() {}

    friend std::ostream& operator<<(std::ostream& os, const ProtoType& t);

    [[nodiscard]] bool is_unary_oper() const {return is_operator_ && args.size() == 1;}
    [[nodiscard]] bool is_binary_oper() const {return is_operator_ && args.size() == 2;}

    [[nodiscard]] std::string get_operator_name() const {
        assert(is_binary_oper() || is_unary_oper());
        return name;
    }

    [[nodiscard]] unsigned get_binary_precedence() const { return precedence_; }
};

struct ExternNode {
    std::unique_ptr<ProtoType> prototype;
    
    explicit ExternNode(std::unique_ptr<ProtoType> _prototype): prototype(std::move(_prototype)) {}

    friend std::ostream& operator<<(std::ostream& os, const ExternNode& t);
};

struct FunctionNode {
    std::unique_ptr<ProtoType> prototype;
    // std::unique_ptr<Expression> body;
    Body body;

    FunctionNode(std::unique_ptr<ProtoType> _prototype, Body _body):
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