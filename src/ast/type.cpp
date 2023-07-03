#include "type.hpp"

#include <cassert>
#include <llvm-15/llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constants.h>

using TypeSystem::Type;

Type TypeSystem::find_type(std::string&& name) {
    if (name == "double") {
        return Type::Double;
    } else if (name == "i32") {
        return Type::Int32;
    }
    assert(false && "Unknown type name");
    //return Type::Error;
}

llvm::Value* TypeSystem::get_type_init_llvm_value(Type type, llvm::LLVMContext& context) {
    switch (type) {
    case TypeSystem::Type::Double:
        return llvm::ConstantFP::get(context, llvm::APFloat(0.0));
    case TypeSystem::Type::Int32:
        return llvm::ConstantInt::get(context, llvm::APInt(32, 0));
    default:
        return nullptr;
    }
}

bool TypeSystem::is_same_type(Type a, Type b) {
    if (a == Type::Any || b == Type::Any) {
        return true;
    } 
    return a == b;
}

std::string TypeSystem::get_type_str(Type type) {
    switch (type) {

    case Type::Double:
        return "double";
    case Type::Int32:
        return "i32";
    case Type::Void:
        return "void";
    case Type::Uninit:
        return "uninit";
    case Type::Error:
        return "error";
    case Type::Any:
        return "any";
    }
    return "";
}

llvm::Type* TypeSystem::get_llvm_type(Type type, llvm::LLVMContext& context) {
    switch (type) {
    case Type::Double:
        return llvm::Type::getDoubleTy(context);
    case Type::Int32:
        return llvm::Type::getInt32Ty(context);
    case Type::Void:
        return llvm::Type::getVoidTy(context);
    default:    
        return nullptr;
    }
}
