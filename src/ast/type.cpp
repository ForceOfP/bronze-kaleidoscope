#include "type.hpp"

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constants.h>

using TypeSystem::Type;

Type TypeSystem::find_type(std::string&& name) {
    if (name == "double") {
        return Type::Double;
    } else if (name == "int32") {
        return Type::Int32;
    }
    return Type::Error;
}

llvm::Value* TypeSystem::get_type_init_value(Type type, llvm::LLVMContext& context) {
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