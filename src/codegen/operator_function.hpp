#pragma once

#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/ADT/Twine.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

using BinaryOperatorFunction = std::function<llvm::Value*(
    llvm::IRBuilder<>*, llvm::Value*, llvm::Value*
)>;

using ExtendedBinaryOperatorFunction = std::function<llvm::Value*(
    llvm::IRBuilder<>*, llvm::Value*, llvm::Value*, llvm::Function*
)>;

struct OperatorFunctionManager {
public:
    using BinaryOperatorFunctionTable = std::unordered_map<std::string, BinaryOperatorFunction>;

    BinaryOperatorFunction get_function(llvm::Type* type, std::string& op);
    bool exist(llvm::Type* type, std::string& op);

    void add_function(llvm::Type* type, std::string& op, llvm::Function* f);
private:
    BinaryOperatorFunction get_function(llvm::Type::TypeID type_id, std::string& op);
    BinaryOperatorFunction get_function(llvm::StringRef struct_name, std::string& op);

    bool exist(llvm::Type::TypeID type_id, std::string& op);
    bool exist(llvm::StringRef struct_name, std::string& op);

    void add_function(llvm::Type::TypeID type_id, std::string& op, llvm::Function* f);
    void add_function(llvm::StringRef struct_name, std::string& op, llvm::Function* f);
private:
    std::unordered_map<std::string, BinaryOperatorFunctionTable> struct_functions_;

    std::unordered_map<llvm::Type::TypeID, BinaryOperatorFunctionTable> primitive_functions_ = {
        {   
            llvm::Type::TypeID::DoubleTyID, {
                {
                    "+", 
                    [](llvm::IRBuilder<>* builder, llvm::Value* lhs, llvm::Value* rhs) -> llvm::Value* {
                        return builder->CreateFAdd(lhs, rhs, "adddouble");
                    }
                },
                {
                    "-",
                    [](llvm::IRBuilder<>* builder, llvm::Value* lhs, llvm::Value* rhs) -> llvm::Value* {
                        return builder->CreateFSub(lhs, rhs, "subdouble");
                    }
                },
                {
                    "*",
                    [](llvm::IRBuilder<>* builder, llvm::Value* lhs, llvm::Value* rhs) -> llvm::Value* {
                        return builder->CreateFMul(lhs, rhs, "muldouble");
                    }
                },
                {
                    "<",
                    [](llvm::IRBuilder<>* builder, llvm::Value* lhs, llvm::Value* rhs) -> llvm::Value* {
                        lhs = builder->CreateFCmpULT(lhs, rhs);
                        return builder->CreateUIToFP(
                            lhs, llvm::Type::getDoubleTy(builder->getContext()), "ltdouble");
                    }
                }
            }
        },
        {
            llvm::Type::TypeID::IntegerTyID, {
                {
                    "+", 
                    [](llvm::IRBuilder<>* builder, llvm::Value* lhs, llvm::Value* rhs) -> llvm::Value* {
                        return builder->CreateAdd(lhs, rhs, "addint");
                    }
                },
                {
                    "-",
                    [](llvm::IRBuilder<>* builder, llvm::Value* lhs, llvm::Value* rhs) -> llvm::Value* {
                        return builder->CreateSub(lhs, rhs, "subint");
                    }
                },
                {
                    "*",
                    [](llvm::IRBuilder<>* builder, llvm::Value* lhs, llvm::Value* rhs) -> llvm::Value* {
                        return builder->CreateMul(lhs, rhs, "mulint");
                    }
                },
                {
                    "<",
                    [](llvm::IRBuilder<>* builder, llvm::Value* lhs, llvm::Value* rhs) -> llvm::Value* {
                        lhs = builder->CreateICmpULT(lhs, rhs, "ltint");
                        return lhs;
                    }
                }
            }
        }
    };
};