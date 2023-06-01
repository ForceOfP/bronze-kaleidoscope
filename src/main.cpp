#include "ast/lexer.hpp"

namespace {
    using namespace std;
}

enum class Stage {Tokens};

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
            auto ans = Lexer::tokenize(input);
            if (stage == Stage::Tokens) {
                for (const auto& token: ans) {
                    cout << token << ' ';
                }
                cout << endl;
                break;
            }
        }
    }
}

int main(int, char**) {
    driver(Stage::Tokens);
    return 0;
}
