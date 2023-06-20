#include "operator_function.hpp"
#include <cassert>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

BinaryOperatorFunction OperatorFunctionManager::get_function(llvm::Type* type, std::string& op) {
    if (type->getTypeID() == llvm::Type::StructTyID) {
        return get_function(type->getStructName(), op);
    } else {
        return get_function(type->getTypeID(), op);
    }
}

BinaryOperatorFunction OperatorFunctionManager::get_function(llvm::Type::TypeID type_id, std::string& op) {
    return primitive_functions_[type_id][op];
}

BinaryOperatorFunction OperatorFunctionManager::get_function(llvm::StringRef struct_name, std::string& op) {
    assert(false && "Unimplement function! Why go to here?");
    return nullptr;
}

bool OperatorFunctionManager::exist(llvm::Type* type, std::string& op) {
    if (type->getTypeID() == llvm::Type::StructTyID) {
        return exist(type->getStructName(), op);
    } else {
        return exist(type->getTypeID(), op);
    }    
}

bool OperatorFunctionManager::exist(llvm::Type::TypeID type_id, std::string& op) {
    if (primitive_functions_.count(type_id)) {
        if (primitive_functions_[type_id].count(op)) {
            return true;
        }
    }
    return false;
}

bool OperatorFunctionManager::exist(llvm::StringRef struct_name, std::string& op) {
    std::string name(struct_name);
    if (struct_functions_.count(name)) {
        if (struct_functions_[name].count(op)) {
            return true;
        }
    }
    return false;
}

void OperatorFunctionManager::add_function(llvm::Type* type, std::string& op, llvm::Function* f) {
    if (type->getTypeID() == llvm::Type::StructTyID) {
        add_function(type->getStructName(), op, f);
    } else {
        add_function(type->getTypeID(), op, f);
    }    
}

void OperatorFunctionManager::add_function(llvm::Type::TypeID type_id, std::string& op, llvm::Function* f) {
    auto functor =
        [f](llvm::IRBuilder<>* builder, llvm::Value* lhs, llvm::Value* rhs) -> llvm::Value* {
        llvm::Value* ops[2] = {lhs, rhs};
        return builder->CreateCall(f, ops, "binop");
    };
    primitive_functions_[type_id][op] = functor;
    //std::cout << "added " << op << std::endl;
}

void OperatorFunctionManager::add_function(llvm::StringRef struct_name, std::string& op, llvm::Function* f) {
    assert(false && "Unimplement function! Why go to here?");
}

