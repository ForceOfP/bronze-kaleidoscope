#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
class SymbolTable {
public:
    SymbolTable() = default;

    void step();
    void back();
    void add_variant(const std::string& name, llvm::AllocaInst* inst);
    void add_constant(const std::string& name, llvm::Value* constant);

    llvm::Value* load(llvm::IRBuilder<>* builder, const std::string& name);
    bool store(llvm::IRBuilder<>* builder, const std::string& name, llvm::Value* target);
private:
    std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> variant_scoped_blocks_;
    std::vector<std::unordered_map<std::string, llvm::Value*>> constant_scoped_blocks_;
};