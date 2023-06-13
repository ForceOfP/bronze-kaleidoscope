#include "ast.hpp"

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
    os << "[Function]: \n" << *(t.prototype);
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
            os << arg << ' ';
        }
    }
    return os;
}

