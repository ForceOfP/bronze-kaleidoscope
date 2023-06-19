#pragma once

#include <llvm/IR/Instructions.h>
#include <llvm/Support/Error.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <map>
#include <memory>
#include <string>

#include "ast/ast.hpp"
#include "jit_engine.hpp"

struct CodeGeneratorSetting {
    bool print_ir = true;
    bool function_pass_optimize = true;
    bool use_jit_else_compile = false;
};

class CodeGenerator {
public:
    explicit CodeGenerator(llvm::raw_ostream& os, bool use_jit);

    llvm::Value* codegen(std::unique_ptr<VarExpr> e);
    llvm::Value* codegen(std::unique_ptr<UnaryExpr> e);
    llvm::Value* codegen(std::unique_ptr<ForExpr> e);
    llvm::Value* codegen(std::unique_ptr<IfExpr> e);
    llvm::Value* codegen(std::unique_ptr<CallExpr> e);
    llvm::Value* codegen(std::unique_ptr<BinaryExpr> e);
    llvm::Value* codegen(std::unique_ptr<VariableExpr> e);
    llvm::Value* codegen(std::unique_ptr<LiteralExpr> e);
    llvm::Value* codegen(std::unique_ptr<Expression> e);

    llvm::Function* codegen(ProtoTypePtr p);
    llvm::Function* codegen(FunctionNode& f);

    void codegen(std::vector<ASTNodePtr>&&);
    void print(std::string&& file_addr);
public:
    std::map<std::string, int> binary_oper_precedence_ = {
        {"=", 2},
        {"<", 10},
        {"+", 20},
        {"-", 20}, 
        {"*", 40}
    };
    CodeGeneratorSetting setting_;
private:
    void initialize_llvm_elements();
    llvm::Function* get_function(std::string& name);
    llvm::AllocaInst* create_entry_block_alloca(llvm::Function* function, const std::string& var_name);
private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::legacy::FunctionPassManager> function_pass_manager_;
    std::unique_ptr<OrcJitEngine> jit_;

    //std::map<std::string, llvm::Value*> named_values_;
    std::map<std::string, llvm::AllocaInst*> named_values_alloca_;
    std::map<std::string, ProtoType> function_protos_ = {};
    std::string err_;
    llvm::ExitOnError exit_on_error_;
    llvm::raw_ostream& output_stream_;
};

