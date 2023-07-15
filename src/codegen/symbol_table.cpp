#include "symbol_table.hpp"
#include "ast/type.hpp"

void SymbolTable::back() {
    variant_scoped_blocks_.pop_back();
    constant_scoped_blocks_.pop_back();
}

void SymbolTable::step() {
    variant_scoped_blocks_.emplace_back();
    constant_scoped_blocks_.emplace_back();
}

void SymbolTable::add_variant(const std::string& name, llvm::AllocaInst* inst, const std::string& type) {
    variant_scoped_blocks_.back().insert({name, {inst, type}});
}

void SymbolTable::add_constant(const std::string& name, llvm::Value* constant, const std::string& type) {
    constant_scoped_blocks_.back().insert({name, {constant, type}});
}

llvm::Value* SymbolTable::load(llvm::IRBuilder<>* builder, const std::string& name) {
    auto c_iter = constant_scoped_blocks_.rbegin();
    for (auto iter = variant_scoped_blocks_.rbegin(); iter != variant_scoped_blocks_.rend(); iter++, c_iter++) {
        if (iter->count(name)) {
            auto [alloca, type] = (*iter)[name];
            if (!type.empty()) {
                return alloca;
            }
            return builder->CreateLoad(alloca->getAllocatedType(), alloca, "alloca");
        }
        if (c_iter->count(name)) {
            return (*c_iter)[name].first;
        }
    }

    return nullptr;
}

bool SymbolTable::store(llvm::IRBuilder<>* builder, const std::string& name, llvm::Value* target) {
    auto c_iter = constant_scoped_blocks_.rbegin();
    for (auto iter = variant_scoped_blocks_.rbegin(); iter != variant_scoped_blocks_.rend(); iter++, c_iter++) {
        if (iter->count(name)) {
            auto [alloca, type] = (*iter)[name];
            builder->CreateStore(target, alloca);
            return true;
        }
        if (c_iter->count(name)) {
            return false;
        }
    }

    return false;
}

bool SymbolTable::store(llvm::IRBuilder<>* builder, const std::string& name, llvm::Value* target, std::vector<llvm::Value*> offsets) {
    auto c_iter = constant_scoped_blocks_.rbegin();
    for (auto iter = variant_scoped_blocks_.rbegin(); iter != variant_scoped_blocks_.rend(); iter++, c_iter++) {
        if (iter->count(name)) {
            auto [alloca, type] = (*iter)[name];

            auto addr = builder->CreateInBoundsGEP(alloca->getAllocatedType(), alloca, offsets, "addr");
            builder->CreateStore(target, addr);
            return true;
        }
        if (c_iter->count(name)) {
            return false;
        }
    }

    return false;    
}

