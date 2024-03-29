#pragma once

#include <any>
#include <cassert>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constant.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class TypeManager;

namespace TypeSystem {

struct TypeBase: public std::enable_shared_from_this<TypeBase> {
    virtual std::string name() = 0;
    virtual llvm::Value* llvm_init_value(llvm::LLVMContext& context) = 0;
    virtual llvm::Type* llvm_type(llvm::LLVMContext& context) = 0;
    virtual llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) = 0;

    virtual bool is_primitive() = 0;
    virtual bool is_aggregate() = 0;
    virtual bool is_data_structure() = 0;

    virtual uint64_t llvm_memory_size(llvm::Module& _module) = 0;
};

struct PrimitiveType: public TypeBase {
    bool is_primitive() final {return true;}
    bool is_aggregate() final {return false;}
    bool is_data_structure() final {return false;}
    uint64_t llvm_memory_size(llvm::Module& _module) override;
private:
    uint64_t memory_size_ = 0;
};

struct AggregateType: public TypeBase {
    bool is_primitive() final {return false;}
    bool is_aggregate() final {return true;}
    bool is_data_structure() final {return false;}
    
    AggregateType(std::string name, std::vector<std::pair<std::string, std::string>>& elements, TypeManager& manager);

    llvm::Value* llvm_init_value(llvm::LLVMContext& context) override;
    llvm::Type* llvm_type(llvm::LLVMContext& context) override;
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override;    
    uint64_t llvm_memory_size(llvm::Module& _module) override;
    
    std::string name() override {return name_;}
    unsigned int element_position(std::string& element);
    TypeBase* element_type(unsigned int index);
    TypeBase* element_type(std::string& element);

    std::string name_;
    std::vector<std::pair<std::string, std::string>> elements_;
private:
    std::vector<std::pair<unsigned int, std::shared_ptr<TypeBase>>> index_with_types_{};
    std::unordered_map<unsigned int, std::string> position_name_{};
    std::unordered_map<std::string, unsigned int> name_position_{};
    std::unordered_map<std::string, TypeBase*> name_type_hash_{};
    std::unordered_map<unsigned int, TypeBase*> index_type_hash_{};

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
    std::string name() override {
        static std::string name = "i32";
        return name;
    }
    llvm::Value* llvm_init_value(llvm::LLVMContext& context) override;
    llvm::Type* llvm_type(llvm::LLVMContext& context) override;
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override;
};

struct DoubleType: public PrimitiveType {
    std::string name() override {
        static std::string name = "double";
        return name;
    }
    llvm::Value* llvm_init_value(llvm::LLVMContext& context) override;
    llvm::Type* llvm_type(llvm::LLVMContext& context) override;
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override;
};

struct VoidType: public PrimitiveType {
    std::string name() override {
        static std::string name = "void";
        return name;
    }
    llvm::Value* llvm_init_value(llvm::LLVMContext& context) override {return nullptr;}
    llvm::Type* llvm_type(llvm::LLVMContext& context) override;
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override {return nullptr;}
    uint64_t llvm_memory_size(llvm::Module& _module) override;
};

struct ArrayType: public DataStructureType {
    std::string name() override {
        std::string name = "array%";
        std::string ans = name + element_type->name() + '%' + std::to_string(length);
        return ans;
    }
    llvm::Value* llvm_init_value(llvm::LLVMContext& context) override;
    llvm::Type* llvm_type(llvm::LLVMContext& context) override;
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override;
    uint64_t llvm_memory_size(llvm::Module& _module) override;

    ArrayType(int len, std::shared_ptr<TypeBase> type): element_type(std::move(type)), length(len) {} 

    std::shared_ptr<TypeBase> element_type;
    int length = 0;
};

struct AnyType: public LogicalType {
    std::string name() override {
        static std::string name = "any";
        return name;
    };
    llvm::Value* llvm_init_value(llvm::LLVMContext& context) override {assert(false && "any type have no init valye");}
    llvm::Type* llvm_type(llvm::LLVMContext& context) override {assert(false && "any type have no llvm type");}
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override {assert(false && "any type have no concrete value");}      
    uint64_t llvm_memory_size(llvm::Module& _module) override {assert(false && "any type have no memory size");};
}; 

struct UninitType: public LogicalType {
    std::string name() override {
        static std::string name = "uninit";
        return name;
    };
    llvm::Value* llvm_init_value(llvm::LLVMContext& context) override {assert(false && "uninit type have no init valye");}
    llvm::Type* llvm_type(llvm::LLVMContext& context) override {assert(false && "uninit type have no llvm type");}
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override {assert(false && "uninit type have no concrete value");}  
    uint64_t llvm_memory_size(llvm::Module& _module) override {assert(false && "any type have no memory size");};
}; 

struct ErrorType: public LogicalType {
    std::string name() override {
        static std::string name = "error";
        return name;
    };
    llvm::Value* llvm_init_value(llvm::LLVMContext& context) override {assert(false && "error type have no init valye");}
    llvm::Type* llvm_type(llvm::LLVMContext& context) override {assert(false && "error type have no llvm type");}
    llvm::Value* get_llvm_value(llvm::LLVMContext& context, std::any value) override {assert(false && "error type have no concrete value");}  
    uint64_t llvm_memory_size(llvm::Module& _module) override {assert(false && "any type have no memory size");};
}; 

bool is_same_type(TypeBase* a, TypeBase* b);
bool is_same_type(std::string& str, TypeBase* t);
bool is_same_type(std::string& str, std::string& name);

std::pair<std::string, int> extract_nesting_type(std::string& name);
 
}  // namespace TypeSystem

using TypePtr = std::shared_ptr<TypeSystem::TypeBase>;
using TypedInstanceName = std::pair<std::string, TypePtr>;

class TypeManager {
public:
    TypeManager();
    std::shared_ptr<TypeSystem::TypeBase> find_type_by_name(std::string& name);
    void add_type(std::string& name, std::vector<std::pair<std::string, std::string>>& _elements);
private:
    std::unordered_map<std::string, std::shared_ptr<TypeSystem::TypeBase>> type_table_;
};