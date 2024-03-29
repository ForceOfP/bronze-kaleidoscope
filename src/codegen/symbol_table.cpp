#include "symbol_table.hpp"
#include "ast/type.hpp"
#include <cassert>
#include <iostream>
#include <llvm-15/llvm/IR/DerivedTypes.h>
#include <llvm-15/llvm/IR/Instructions.h>
#include <llvm-15/llvm/IR/Type.h>
#include <llvm-15/llvm/IR/Value.h>
#include <variant>

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

bool SymbolTable::store(llvm::IRBuilder<>* builder, const std::string& name, llvm::Value* target, std::vector<ElementOrOffset> addrs, TypeManager& type_manager) {
    auto c_iter = constant_scoped_blocks_.rbegin();
    for (auto iter = variant_scoped_blocks_.rbegin(); iter != variant_scoped_blocks_.rend(); iter++, c_iter++) {
        if (iter->count(name)) {
            auto& [alloca, type] = (*iter)[name];
            auto front_end_type = type_manager.find_type_by_name(type);
            auto front_end_type_raw = front_end_type.get();

            llvm::Value* result_ptr = nullptr;
            llvm::AllocaInst* target_ptr = alloca;
            llvm::Type* target_type = alloca->getAllocatedType();

            for (auto& addr: addrs) {
                std::vector<llvm::Value*> offset_values {llvm::ConstantInt::get(builder->getContext(), llvm::APInt(32, 0))};
                if (std::holds_alternative<llvm::Value*>(addr)) {
                    if (auto arr = static_cast<TypeSystem::ArrayType*>(front_end_type_raw)) {
                        front_end_type_raw = arr->element_type.get();
                    } else {
                        assert(false && "need array type in store");
                    }
                    auto& offset = std::get<llvm::Value*>(addr);
                    offset_values.push_back(offset);
                    result_ptr = builder->CreateInBoundsGEP(target_type, target_ptr, offset_values, "addr");
                    target_type = front_end_type_raw->llvm_type(builder->getContext());
                    target_ptr = static_cast<llvm::AllocaInst*>(result_ptr);
                } else {
                    auto element_name = std::get<std::string>(addr);
                    if (auto aggr = static_cast<TypeSystem::AggregateType*>(front_end_type_raw)) {
                        front_end_type_raw = aggr->element_type(element_name);
                        auto element_index = aggr->element_position(element_name);
                        offset_values.push_back(llvm::ConstantInt::get(builder->getContext(), llvm::APInt(32, element_index)));
                        result_ptr = builder->CreateInBoundsGEP(target_type, target_ptr, offset_values, "addr");                    
                        target_type = target_type->getStructElementType(element_index);
                        target_ptr = static_cast<llvm::AllocaInst*>(result_ptr);
                    } else {
                        assert(false && "need array type in store");
                    }
                }
            }
            builder->CreateStore(target, result_ptr);

            return true;
        }
        if (c_iter->count(name)) {
            return false;
        }
    }

    return false;      
}

std::string& SymbolTable::find_symbol_type_str(std::string& symbol_name) {
    static std::string emp = "";
    
    auto c_iter = constant_scoped_blocks_.rbegin();
    
    for (auto iter = variant_scoped_blocks_.rbegin(); iter != variant_scoped_blocks_.rend(); iter++, c_iter++) {
        if (iter->count(symbol_name)) {
            return (*iter)[symbol_name].second;
        }
        if (c_iter->count(symbol_name)) {
            return emp;
        }
    }

    return emp;    
}

