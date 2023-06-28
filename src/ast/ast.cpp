#include "ast.hpp"
#include <ostream>
#include <vector>

std::ostream& operator<<(std::ostream& os, const Body& body);
std::ostream& operator<<(std::ostream& os, const ExpressionPtr& e);

std::ostream& operator<<(std::ostream& os, ASTNode& t) {
    t.match(
        [&](const ExternNode& e){
            os << e;
        }, 
        [&](const FunctionNode& f){
            os << f;
        }
    );
    return os;
}

std::ostream& operator<<(std::ostream& os, const FunctionNode& t) {
    os << "[Function]: \n" << *(t.prototype) << '\n' << t.body;
    return os;
}

std::ostream& operator<<(std::ostream& os, const ExternNode& t) {
    os << "[Extern]: \n" << *(t.prototype);
    return os;
}

std::ostream& operator<<(std::ostream& os, const ProtoType& t) {
    os << "\t[Name]: " << t.name << '\n';
    if (!t.args.empty()) {
        os << "\t[Args]: " ;
        for (auto& arg: t.args) {
            os << arg.first << ' ';
        }
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const Body& body) {
    for (const auto& line: body) {
        os << line;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const ExpressionPtr& e) {
    os << "\t[Body]: " << '\n';
    Expression* raw = e.get();
    auto l = dynamic_cast<LiteralExpr*>(raw);
    if (l) {
        os << "\t\t[Literal]\n";
    }

    auto c = dynamic_cast<CallExpr*>(raw);
    if (c) {
        os << "\t\t[Call]\n";
    }
    
    auto v = dynamic_cast<VariableExpr*>(raw);
    if (v) {
        os << "\t\t[Variable]\n";
    }
    
    auto b = dynamic_cast<BinaryExpr*>(raw);
    if (b) {
        os << "\t\t[Binary]\n";
    }

    auto i = dynamic_cast<IfExpr*>(raw);
    if (i) {
        os << "\t\t[If]\n";    
    }

    auto f = dynamic_cast<ForExpr*>(raw);
    if (f) {
        os << "\t\t[For]\n";
    }

    auto u = dynamic_cast<UnaryExpr*>(raw);
    if (u) {
        os << "\t\t[Unary]\n";   
    }
    auto var = dynamic_cast<VarExpr*>(raw);
    if (var) {
        os << "\t\t[Var]\n";
    }

    auto r = dynamic_cast<ReturnExpr*>(raw);
    if (r) {
        os << "\t\t[Return]\n";
    }

    return os;
}