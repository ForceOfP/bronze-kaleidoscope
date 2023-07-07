#include "type.hpp"

#include <cassert>
#include <iostream>
#include <iterator>
#include <llvm-15/llvm/IR/Constant.h>
#include <llvm-15/llvm/IR/Constants.h>
#include <llvm-15/llvm/IR/DerivedTypes.h>
#include <llvm-15/llvm/IR/Type.h>
#include <llvm-15/llvm/IR/Value.h>
#include <llvm-15/llvm/Support/Casting.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constants.h>
#include <memory>
#include <string>
#include <vector>

namespace TypeSystem {

llvm::Value* Int32Type::llvm_init_name(llvm::LLVMContext& context) {
    return llvm::ConstantInt::get(context, llvm::APInt(32, 0));
}

llvm::Type* Int32Type::llvm_type(llvm::LLVMContext& context) {
    return llvm::Type::getInt32Ty(context);
}

llvm::Value* Int32Type::get_llvm_value(llvm::LLVMContext& context, std::any value) {
    auto taked = std::any_cast<double>(value);
    return llvm::ConstantInt::get(context, llvm::APInt(32, static_cast<int>(taked)));
}

llvm::Value* DoubleType::llvm_init_name(llvm::LLVMContext& context) {
    return llvm::ConstantFP::get(context, llvm::APFloat(0.0));
}

llvm::Type* DoubleType::llvm_type(llvm::LLVMContext& context) {
    return llvm::Type::getDoubleTy(context);
}

llvm::Value* DoubleType::get_llvm_value(llvm::LLVMContext& context, std::any value) {
    auto taked = std::any_cast<double>(value);
    return llvm::ConstantFP::get(context, llvm::APFloat(taked));
}

llvm::Type* VoidType::llvm_type(llvm::LLVMContext& context) {
    return llvm::Type::getVoidTy(context);
}

llvm::Value* ArrayType::llvm_init_name(llvm::LLVMContext& context) {
    auto element_value = element_type->llvm_init_name(context);
    auto casted_value = static_cast<llvm::Constant*>(element_value);
    std::vector<llvm::Value*> values(length, casted_value);
    auto array_type = this->llvm_type(context);
    auto casted_type = static_cast<llvm::ArrayType*>(array_type);
    return llvm::ConstantArray::get(casted_type, casted_value);
}

llvm::Type* ArrayType::llvm_type(llvm::LLVMContext& context) {
    return llvm::ArrayType::get(this->element_type->llvm_type(context), length);
}

llvm::Value* ArrayType::get_llvm_value(llvm::LLVMContext& context, std::any value) {
    return nullptr;
}

std::unique_ptr<TypeBase> find_type_by_name(std::string&& name) {
    if (name == "double") {
        return std::make_unique<DoubleType>();
    } else if (name == "i32") {
        return std::make_unique<Int32Type>();
    } else if (name == "any") {
        return std::make_unique<AnyType>();
    } else if (name == "uninit") {
        return std::make_unique<UninitType>();
    } else if (name == "error") {
        return std::make_unique<ErrorType>();
    } else if (name.starts_with("array")) {
        auto iter1 = name.begin();
        while (*iter1 != '%') {
            iter1++;
        }
        auto iter2 = name.end() - 1;
        while (*iter2 != '%') {
            iter2--;
        }

        std::string element_name = name.substr(iter1 - name.begin() + 1, std::distance(iter1, iter2));
        std::string size_str = name.substr(std::distance(name.begin(), iter2) + 1);
        int array_size = std::stoi(size_str);
        auto type_ptr = find_type_by_name(std::move(element_name));
        return std::make_unique<ArrayType>(array_size, std::move(type_ptr));
    }

    std::cout << "type name is: " << name << std::endl;
    assert(false && "Unknown type name");
}

bool is_same_type(TypeBase* a, TypeBase* b) {
    if (!a) {
        assert(false && "lhs is empty");
    }
    if (!b) {
        assert(false && "rhs is empty");
    }

    if (a->name() == "any" || b->name() == "any") {
        return true;
    } 
    return a->name() == b->name();
}

bool is_same_type(std::string& str, TypeBase* t) {
    if (!t) {
        assert(false && "t is empty");
    }
    if (str == "any" || t->name() == "any") {
        return true;
    } 
    return str == t->name();
}

bool is_same_type(std::string& str, std::string& name) {
    if (str == "any" || name == "any") {
        return true;
    }  
    return str == name;
}

}  // namespace TypeSystem
/* 
using TypeSystem::TypeEnum;

TypeEnum TypeSystem::find_type(std::string&& name) {
    if (name == "double") {
        return TypeEnum::Double;
    } else if (name == "i32") {
        return TypeEnum::Int32;
    } else if (name == "i32Array") {
        return TypeEnum::ArrayInt32;
    } else if (name == "doubleArray") {
        return TypeEnum::ArrayDouble;
    }
    assert(false && "Unknown type name");
    //return Type::Error;
}

llvm::Value* TypeSystem::get_type_init_llvm_value(TypeEnum type, llvm::LLVMContext& context) {
    switch (type) {
    case TypeSystem::TypeEnum::Double:
        return llvm::ConstantFP::get(context, llvm::APFloat(0.0));
    case TypeSystem::TypeEnum::Int32:
        return llvm::ConstantInt::get(context, llvm::APInt(32, 0));
    // scase TypeSystem::Type::ArrayDouble:
    //     return llvm::ConstantFP::get(context, llvm::APFloat(0.0));
    // case TypeSystem::Type::ArrayInt32:
    //     return llvm::ConstantInt::get(context, llvm::APInt(32, 0));
    default:
        return nullptr;
    }
}

bool TypeSystem::is_same_type(TypeEnum a, TypeEnum b) {
    if (a == TypeEnum::Any || b == TypeEnum::Any) {
        return true;
    } 
    return a == b;
}

std::string TypeSystem::get_type_str(TypeEnum type) {
    switch (type) {

    case TypeEnum::Double:
        return "double";
    case TypeEnum::Int32:
        return "i32";
    case TypeEnum::Void:
        return "void";
    case TypeEnum::Uninit:
        return "uninit";
    case TypeEnum::Error:
        return "error";
    case TypeEnum::Any:
        return "any";
    case TypeEnum::ArrayInt32:
        return "i32Array";
    case TypeEnum::ArrayDouble:
        return "doubleArray";
    }
    return "";
}

llvm::Type* TypeSystem::get_llvm_type(TypeEnum type, llvm::LLVMContext& context) {
    switch (type) {
    case TypeEnum::Double:
        return llvm::Type::getDoubleTy(context);
    case TypeEnum::Int32:
        return llvm::Type::getInt32Ty(context);
    case TypeEnum::Void:
        return llvm::Type::getVoidTy(context);
    //case Type::ArrayDouble:
    //    auto element_type = llvm::Type::getDoubleTy(context);;
    //    return llvm::ArrayType::get()
    //    break;
    //case Type::ArrayInt32:
    //    return llvm::ArrayType::getInt32Ty(context);
    //    break;
    default:    
        return nullptr;
    }
}
 */