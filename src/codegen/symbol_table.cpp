#include "symbol_table.hpp"

void SymbolTable::back() {
    // todo
    scoped_blocks_.pop_back();
}

void SymbolTable::step() {
    scoped_blocks_.emplace_back();
}

void SymbolTable::add_local(const std::string& name, llvm::AllocaInst* inst) {
    scoped_blocks_.back().insert({name, inst});
}

void SymbolTable::add_global(const std::string& name, llvm::GlobalValue* value) {
    global_values_.insert({name, value});
}

llvm::Value* SymbolTable::find(const std::string& name) {
    for (auto iter = scoped_blocks_.rbegin(); iter != scoped_blocks_.rend(); iter++) {
        if (iter->count(name)) {
            return (*iter)[name];
        }
    }

    if (global_values_.count(name)) {
        return global_values_[name];
    }

    return nullptr;
}

