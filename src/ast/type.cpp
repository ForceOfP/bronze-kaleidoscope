#include "type.hpp"

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constants.h>

using TypeSystem::Type;

Type TypeSystem::get_type(std::string&& name) {
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