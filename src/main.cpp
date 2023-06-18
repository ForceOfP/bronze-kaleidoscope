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
// def binary : 1 (x, y) y;
// def test(x) printd(x) : x = 4 : printd(x);
// test(123);

// def binary : 1 (x, y) y;
// def fibi(x) var a = 1, b = 1, c in (for i = 3, i < x in c = a + b : a = b : b = c) : b;
// def test2(x) var a = 1, b = 2 in (a = b) : a;