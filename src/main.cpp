#include "ast/lexer.hpp"
#include "ast/parser.hpp"
#include "ast/token.hpp"

namespace {
    using namespace std;
}

enum class Stage {Tokens, Parser};

void driver(Stage stage) {
    string input;
    for (;;) {
        cout << "> ";
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
            auto parser = Parser(std::move(tokens));
            
            parser.parse();
            break;
        }
    }
}

int main(int, char**) {
    driver(Stage::Parser);
    return 0;
}
