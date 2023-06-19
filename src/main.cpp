#include "ast/lexer.hpp"
#include "ast/parser.hpp"
#include "ast/token.hpp"
#include "codegen/codegen.hpp"
#include <llvm-c/Target.h>
#include <llvm/Support/raw_ostream.h>

namespace {
    using namespace std;
}

enum class Stage {Tokens, Parser, Codegen, Target};

void driver(Stage stage) {
    string input;
    auto generator = CodeGenerator(llvm::errs(), stage != Stage::Target); 
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

    if (stage == Stage::Target) {
        generator.print("./target/output.o");
    }
}

int main(int, char**) {
    auto state = Stage::Target;

    if (state != Stage::Target) {
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();
        LLVMInitializeNativeAsmParser();
    }
    driver(state);

    return 0;
}