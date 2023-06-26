#include "symbol_table.hpp"

void SymbolTable::back() {
    variant_scoped_blocks_.pop_back();
    constant_scoped_blocks_.pop_back();
}

void SymbolTable::step() {
    variant_scoped_blocks_.emplace_back();
    constant_scoped_blocks_.emplace_back();
}

void SymbolTable::add_variant(const std::string& name, llvm::AllocaInst* inst) {
    variant_scoped_blocks_.back().insert({name, inst});
}

void SymbolTable::add_constant(const std::string& name, llvm::Value* constant) {
    constant_scoped_blocks_.back().insert({name, constant});
}

llvm::Value* SymbolTable::load(llvm::IRBuilder<>* builder, const std::string& name) {
    auto c_iter = constant_scoped_blocks_.rbegin();
    for (auto iter = variant_scoped_blocks_.rbegin(); iter != variant_scoped_blocks_.rend(); iter++, c_iter++) {
        if (iter->count(name)) {
            auto alloca = (*iter)[name];
            return builder->CreateLoad(alloca->getAllocatedType(), alloca, "alloca");
        }
        if (c_iter->count(name)) {
            return (*c_iter)[name];
        }
    }

    return nullptr;
}

bool SymbolTable::store(llvm::IRBuilder<>* builder, const std::string& name, llvm::Value* target) {
    auto c_iter = constant_scoped_blocks_.rbegin();
    for (auto iter = variant_scoped_blocks_.rbegin(); iter != variant_scoped_blocks_.rend(); iter++, c_iter++) {
        if (iter->count(name)) {
            auto alloca = (*iter)[name];
            builder->CreateStore(target, alloca);
            return true;
        }
        if (c_iter->count(name)) {
            return false;
        }
    }

    return false;
}
