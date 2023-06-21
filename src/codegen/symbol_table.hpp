#pragma once

#include <llvm-15/llvm/IR/GlobalValue.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <unordered_map>
#include <vector>
class SymbolTable {
public:
    SymbolTable() = default;

    void step();
    void back();
    void add_local(const std::string& name, llvm::AllocaInst* inst);
    void add_global(const std::string& name, llvm::GlobalValue* value);
    llvm::Value* find(const std::string& name);

private:
    std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> scoped_blocks_;
    std::unordered_map<std::string, llvm::GlobalValue*> global_values_;
};