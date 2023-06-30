#include "ast/lexer.hpp"
#include "ast/parser.hpp"
#include "ast/semantic.hpp"
#include "ast/token.hpp"
#include "codegen/codegen.hpp"
#include "codegen/jit_codegen.hpp"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <llvm-15/llvm/Support/raw_ostream.h>
#include <llvm-c/Target.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

namespace {
    using namespace std;
}

enum class Stage {Tokens, Parser, Codegen, Target, FileIO};

void file_driver(std::string&& file_input, std::string&& file_output) {
    std::error_code ec;
    llvm::raw_fd_ostream output(file_output, ec, llvm::sys::fs::OF_None);
    if (ec) {
        cout << "Could not open file: " << ec.message() << '\n';\
        exit(1);
    }
    CodeGeneratorSetting setting = {
        .print_ir = true,
        .function_pass_optimize = true,
    };

    auto generator = CodeGenerator(output, setting);

    std::ifstream ifs(file_input);
    std::string line;

    while (std::getline(ifs, line, ';')) { // use ';' as delimeter
        auto tokens = Lexer::tokenize(line);
        tokens.emplace_back(TokenType::Eof);

        auto parser = Parser(std::move(tokens), generator.binary_oper_precedence_);
        auto asts = parser.parse();
        generator.codegen(std::move(asts));
    }
}

void driver(Stage stage) {
    string input;
    CodeGenerator* generator = nullptr;
    CodeGeneratorSetting setting = {
        .print_ir = true,
        .function_pass_optimize = true,
    };

    if (stage == Stage::Target) {
        generator = new CodeGenerator(llvm::errs(), setting);
    } else {
        generator = new JitCodeGenerator(llvm::errs(), setting);
    }
    //auto generator = CodeGenerator(llvm::errs(), stage != Stage::Target); 
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

            auto parser = Parser(std::move(tokens), generator->binary_oper_precedence_);
            auto asts = parser.parse();
            if (stage == Stage::Parser) {
                for (auto& ast: asts) {
                    cout << *ast << endl;
                }
            }
            
            TypeChecker checker;

            for (auto& ast: asts) {
                if (!checker.check(*ast)) {
                    cout << "TypeChecker failed at " << *ast << endl;
                }
            }            

            generator->codegen(std::move(asts));

            break;
        }
    }

    if (stage == Stage::Target) {
        generator->print("./target/output.o");
    }

    delete(generator);
}

int main(int argv, char** args) {
    auto state = Stage::Codegen;

    if (state == Stage::Target) {
        assert(argv == 3);
        file_driver(args[1], args[2]);
        return 0;
    }

    if (state != Stage::Target) {
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();
        LLVMInitializeNativeAsmParser();
    }
    driver(state);

    return 0;
}