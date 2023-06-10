#pragma once

#include <variant>

// helper type for the visitor
template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// a sample for rustic match
/* template <typename Val, typename... Ts>
auto match(Val val, Ts... ts) {
    return std::visit(overloaded{ts...}, val);
} */