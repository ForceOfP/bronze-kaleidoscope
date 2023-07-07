#pragma once

#include <any>
#include <llvm-15/llvm/IR/LLVMContext.h>
#include <llvm-15/llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constant.h>
#include <memory>
#include <string>
#include <utility>

namespace TypeSystem {

struct TypeBase {
    virtual std::string& name() = 0;
    virtual llvm::Value* llvm_init_name(llvm::LLVMContext& context) = 0;
    virtual llvm::Type* llvm_type(llvm::LLVMContext& context) = 0;
    virtual llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) = 0;

    virtual bool is_primitive() = 0;
    virtual bool is_aggregate() = 0;
    virtual bool is_data_structure() = 0;
};

struct PrimitiveType: public TypeBase {
    bool is_primitive() final {return true;}
    bool is_aggregate() final {return false;}
    bool is_data_structure() final {return false;}
};

struct AggregateType: public TypeBase {
    bool is_primitive() final {return false;}
    bool is_aggregate() final {return true;}
    bool is_data_structure() final {return false;}
};

struct DataStructureType: public TypeBase {
    bool is_primitive() final {return false;}
    bool is_aggregate() final {return false;}
    bool is_data_structure() final {return true;}  
};

struct LogicalType: public TypeBase {
    bool is_primitive() final {return false;}
    bool is_aggregate() final {return false;}
    bool is_data_structure() final {return false;}     
};

struct Int32Type: public PrimitiveType {
    std::string& name() override {
        static std::string name = "i32";
        return name;
    }
    llvm::Value* llvm_init_name(llvm::LLVMContext& context) override;
    llvm::Type* llvm_type(llvm::LLVMContext& context) override;
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override;
};

struct DoubleType: public PrimitiveType {
    std::string& name() override {
        static std::string name = "double";
        return name;
    }
    llvm::Value* llvm_init_name(llvm::LLVMContext& context) override;
    llvm::Type* llvm_type(llvm::LLVMContext& context) override;
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override;
};

struct VoidType: public PrimitiveType {
    std::string& name() override {
        static std::string name = "void";
        return name;
    }
    llvm::Value* llvm_init_name(llvm::LLVMContext& context) override {return nullptr;}
    llvm::Type* llvm_type(llvm::LLVMContext& context) override;
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override {return nullptr;}
};

struct ArrayType: public DataStructureType {
    std::string& name() override {
        static std::string name = "array%";
        static std::string ans = name + element_type->name() + '%' + std::to_string(length);
        return ans;
    }
    llvm::Value* llvm_init_name(llvm::LLVMContext& context) override;
    llvm::Type* llvm_type(llvm::LLVMContext& context) override;
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override;

    ArrayType(int len, std::unique_ptr<TypeBase> type): element_type(std::move(type)), length(len) {} 

    std::unique_ptr<TypeBase> element_type;
    int length = 0;
};

struct AnyType: public LogicalType {
    std::string& name() override {
        static std::string name = "any";
        return name;
    };
    llvm::Value* llvm_init_name(llvm::LLVMContext& context) override { return nullptr; }
    llvm::Type* llvm_type(llvm::LLVMContext& context) override { return nullptr; }   
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override { return nullptr; }
}; 

struct UninitType: public LogicalType {
    std::string& name() override {
        static std::string name = "uninit";
        return name;
    };
    llvm::Value* llvm_init_name(llvm::LLVMContext& context) override { return nullptr; }
    llvm::Type* llvm_type(llvm::LLVMContext& context) override { return nullptr; }   
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override { return nullptr; }
}; 

struct ErrorType: public LogicalType {
    std::string& name() override {
        static std::string name = "error";
        return name;
    };
    llvm::Value* llvm_init_name(llvm::LLVMContext& context) override { return nullptr; }
    llvm::Type* llvm_type(llvm::LLVMContext& context) override { return nullptr; }   
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override { return nullptr; }
}; 

std::unique_ptr<TypeBase> find_type_by_name(std::string&& name);
bool is_same_type(TypeBase* a, TypeBase* b);
bool is_same_type(std::string& str, TypeBase* t);
bool is_same_type(std::string& str, std::string& name);
/* enum class TypeEnum {
    Double,
    Int32,
    Void,
    ArrayInt32,
    ArrayDouble,
    
    // use for TypeSystem logic
    Uninit,
    Error,
    Any,
};


TypeEnum find_type(std::string&& name);
bool is_same_type(TypeEnum a, TypeEnum b);
std::string get_type_str(TypeEnum type);

llvm::Value* get_type_init_llvm_value(TypeEnum type, llvm::LLVMContext& context);
llvm::Type* get_llvm_type(TypeEnum type, llvm::LLVMContext& context); */
 
}  // namespace TypeSystem

using TypePtr = std::unique_ptr<TypeSystem::TypeBase>;
using TypedInstanceName = std::pair<std::string, TypePtr>;
