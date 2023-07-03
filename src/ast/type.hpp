#pragma once

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constant.h>
#include <string>
#include <utility>

namespace TypeSystem {

enum class Type {
    Double,
    Int32,
    Void,
    
    // use for TypeSystem logic
    Uninit,
    Error,
    Any,
};

using TypedInstanceName = std::pair<std::string, Type>;

Type find_type(std::string&& name);
bool is_same_type(Type a, Type b);
std::string get_type_str(Type type);

llvm::Value* get_type_init_llvm_value(Type type, llvm::LLVMContext& context);
llvm::Type* get_llvm_type(Type type, llvm::LLVMContext& context);
 
}  // namespace TypeSystem