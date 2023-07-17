#include "type.hpp"

#include <any>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Casting.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constants.h>
#include <memory>
#include <string>
#include <vector>

namespace TypeSystem {

uint64_t PrimitiveType::llvm_memory_size(llvm::Module& _module) {
    if (memory_size_) {
        return memory_size_;
    }
    auto type = llvm_type(_module.getContext());
    memory_size_ = _module.getDataLayout().getTypeAllocSize(type);
    return memory_size_;
}

uint64_t VoidType::llvm_memory_size(llvm::Module& _module) {
    return 0;
}

llvm::Value* Int32Type::llvm_init_value(llvm::LLVMContext& context) {
    return llvm::ConstantInt::get(context, llvm::APInt(32, 0));
}

llvm::Type* Int32Type::llvm_type(llvm::LLVMContext& context) {
    return llvm::Type::getInt32Ty(context);
}

llvm::Value* Int32Type::get_llvm_value(llvm::LLVMContext& context, std::any value) {
    auto taked = std::any_cast<double>(value);
    return llvm::ConstantInt::get(context, llvm::APInt(32, static_cast<int>(taked)));
}

llvm::Value* DoubleType::llvm_init_value(llvm::LLVMContext& context) {
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

llvm::Value* ArrayType::llvm_init_value(llvm::LLVMContext& context) {
    auto element_value = element_type->llvm_init_value(context);
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
    auto taked = std::any_cast<std::vector<llvm::Constant*>&>(value);
    auto array_type = this->llvm_type(context);
    auto casted_type = static_cast<llvm::ArrayType*>(array_type);
    return llvm::ConstantArray::get(casted_type, taked);
}

uint64_t ArrayType::llvm_memory_size(llvm::Module& _module) {
    return length * element_type->llvm_memory_size(_module);
}

AggregateType::AggregateType(
    std::string name,
    std::vector<std::pair<std::string, std::string>>& _elements,
    std::unordered_map<std::string, TypeSystem::AggregateType>& struct_table)
    : name_(std::move(name)), elements_(_elements) {
    int posi = 0;
    for (auto& [_name, type_str]: elements_) {
        auto type = find_type_by_name(std::move(type_str), struct_table);
        index_with_types_.emplace_back(posi, std::move(type));
        position_name_.insert({posi, _name});
        name_position_.insert({_name, posi});
        posi++;
    }

    for (auto& [index, type_ptr]: index_with_types_) {
        name_type_hash_.insert({position_name_[index], type_ptr.get()});
        index_type_hash_.insert({index, type_ptr.get()});
    }
}

llvm::Value* AggregateType::llvm_init_value(llvm::LLVMContext& context) {
    std::vector<llvm::Constant*> values;
    values.reserve(index_with_types_.size());
    for (auto& [_, type]: index_with_types_) {
        values.push_back(static_cast<llvm::Constant*>(type->llvm_init_value(context)));
    }    
    llvm::Type* struct_type = llvm_type(context);
    auto casted_type = static_cast<llvm::StructType*>(struct_type); 
    return llvm::ConstantStruct::get(casted_type, values);
}

llvm::Type* AggregateType::llvm_type(llvm::LLVMContext& context) {
    std::vector<llvm::Type*> types;
    types.reserve(index_with_types_.size());
    for (auto& [_, type]: index_with_types_) {
        types.push_back(type->llvm_type(context));
    }
    return llvm::StructType::create(context, types, name_);
}

llvm::Value* AggregateType::get_llvm_value(llvm::LLVMContext& context, std::any value) {
    auto taked = std::any_cast<std::vector<llvm::Constant*>&>(value);
    auto struct_type = this->llvm_type(context);
    auto casted_type = static_cast<llvm::StructType*>(struct_type);
    return llvm::ConstantStruct::get(casted_type, taked);
}

uint64_t AggregateType::llvm_memory_size(llvm::Module& _module) {
    uint64_t len = 0;
    for (auto& [_, type]: index_with_types_) {
        len += type->llvm_memory_size(_module);
    }
    return len;
}

unsigned int AggregateType::element_position(std::string& element) {
    return name_position_[element];
}

TypeBase* AggregateType::element_type(std::string& element) {
    return name_type_hash_[element];
}

std::unique_ptr<TypeBase> find_type_by_name(std::string&& name, std::unordered_map<std::string, TypeSystem::AggregateType>& struct_table_) {
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
        auto [element_name, array_size] = extract_nesting_type(name);
        auto type_ptr = find_type_by_name(std::move(element_name), struct_table_);
        return std::make_unique<ArrayType>(array_size, std::move(type_ptr));
    } else if (struct_table_.count(name)) {
        auto& struct_type = struct_table_.at(name);
        return std::make_unique<AggregateType>(struct_type.name_, struct_type.elements_, struct_table_);
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

std::pair<std::string, int> extract_nesting_type(std::string& name) {
    if (name == "any") {
        return {"any", -1};
    }
    
    auto iter1 = name.begin();
    while (*iter1 != '%') {
        iter1++;
    }
    auto iter2 = name.end() - 1;
    while (*iter2 != '%') {
        iter2--;
    }

    std::string element_name = name.substr(iter1 - name.begin() + 1, std::distance(iter1, iter2) - 1);
    std::string size_str = name.substr(std::distance(name.begin(), iter2) + 1);
    int array_size = std::stoi(size_str);    
    return {element_name, array_size};
}

}  // namespace TypeSystem
