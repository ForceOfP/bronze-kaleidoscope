#include "ast/lexer.hpp"
#include "ast/parser.hpp"
#include "ast/token.hpp"
#include "codegen/codegen.hpp"
#include <llvm/Support/raw_ostream.h>

namespace {
    using namespace std;
}

enum class Stage {Tokens, Parser, Codegen};

void driver(Stage stage) {
    string input;
    auto generator = CodeGenerator(llvm::errs()); 
    for (;;) {
        cout << "ready> ";
        cout.flush();
        input.clear();
        getline(cin, input);
        if (input == ".quit") {
            break;
        }

        for (;;) {
            auto tokens = Lexer::tokenize(input);
            if (stage == Stage::Tokens) {
                for (const auto& token: tokens) {
                    cout << token << ' ';
                }
                cout << endl;
                break;
            } 
            tokens.emplace_back(TokenType::Eof);

            auto parser = Parser(std::move(tokens), generator.binary_oper_precedence_);
            auto asts = parser.parse();
            if (stage == Stage::Parser) {
                for (auto& ast: asts) {
                    cout << *ast << endl;
                }
            }

            generator.codegen(std::move(asts));

            break;
        }
    }
}

int main(int, char**) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    driver(Stage::Codegen);
    return 0;
}

// extern printd(x);
// def binary : 1 (x, y) 0;
// printd(123) : printd(456) : printd(789);

// def unary - (v) 0 - v;
// -(2+3)

// def unary ! (v) if v then 0 else 1;
// !1
// !0

// def binary | 5 (LHS, RHS) if LHS then 1 else if RHS then 1 else 0;
// def binary & 6 (LHS, RHS) if !LHS then 0 else !!RHS;

// def binary > 10 (LHS, RHS) RHS < LHS;

// def binary == 9 (LHS, RHS) !(LHS < RHS | LHS > RHS);

// def binary : 1 (x, y) y;

// extern putchard(char);

// def printdensity(d) if d > 8 then putchard(32) else if d > 4 then putchard(46) else if d > 2 then putchard(43) else putchard(42);
// printdensity(1): printdensity(2): printdensity(3):printdensity(4): printdensity(5): printdensity(9):putchard(10);