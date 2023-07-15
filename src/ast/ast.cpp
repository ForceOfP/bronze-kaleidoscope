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
    os << "\t[Body]: " << '\n';
    for (const auto& line: body.data) {
        os << line;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const ExpressionPtr& e) {
    os << "\t\t" << e->expression_name() << '\n';
    return os;
}