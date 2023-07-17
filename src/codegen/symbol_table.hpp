#pragma once

#include <llvm-15/llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "ast/type.hpp"
class SymbolTable {
public:
    SymbolTable() = default;

    void step();
    void back();
    void add_variant(const std::string& name, llvm::AllocaInst* inst, const std::string& type = "");
    void add_constant(const std::string& name, llvm::Value* constant, const std::string& type = "");

    llvm::Value* load(llvm::IRBuilder<>* builder, const std::string& name);
    bool store(llvm::IRBuilder<>* builder, const std::string& name, llvm::Value* target);
    bool store(llvm::IRBuilder<>* builder, const std::string& name, llvm::Value* target, std::vector<llvm::Value*> offsets);

    using ElementOrOffset = std::variant<std::string, llvm::Value*>;
    bool store(llvm::IRBuilder<>* builder, const std::string& name, llvm::Value* target, std::vector<ElementOrOffset> addrs, std::unordered_map<std::string, TypeSystem::AggregateType>& struct_table_);
    std::string& find_symbol_type_str(std::string& symbol_name);
private:
    std::vector<std::unordered_map<std::string, std::pair<llvm::AllocaInst*, std::string>>> variant_scoped_blocks_;
    std::vector<std::unordered_map<std::string, std::pair<llvm::Value*, std::string>>> constant_scoped_blocks_;
};