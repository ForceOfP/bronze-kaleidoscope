#pragma once

#include "ast/ast.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <map>
#include <memory>
#include <string>

class CodeGenerator {
public:
    explicit CodeGenerator(std::vector<ASTNodePtr>&&);

    llvm::Value* codegen(std::unique_ptr<CallExpr> e);
    llvm::Value* codegen(std::unique_ptr<BinaryExpr> e);
    llvm::Value* codegen(std::unique_ptr<VariableExpr> e);
    llvm::Value* codegen(std::unique_ptr<LiteralExpr> e);
    llvm::Value* codegen(std::unique_ptr<Expression> e);

    llvm::Function* codegen(ProtoTypePtr p);
    llvm::Function* codegen(FunctionNode& f);

    void codegen();
private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    std::unique_ptr<llvm::Module> module_;
    std::map<std::string, llvm::Value*> named_values_;
    std::string err_;
    std::vector<ASTNodePtr> ast_tree_;
};