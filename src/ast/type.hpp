#pragma once

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
    Uninit,
    Error,
};

using TypedInstanceName = std::pair<std::string, Type>;

Type find_type(std::string&& name);
llvm::Value* get_type_init_value(Type type, llvm::LLVMContext& context);

}  // namespace TypeSystem