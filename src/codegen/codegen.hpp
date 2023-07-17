#pragma once

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Error.h>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "ast/ast.hpp"
#include "ast/type.hpp"
#include "jit_engine.hpp"
#include "operator_function.hpp"
#include "symbol_table.hpp"

struct CodeGeneratorSetting {
    bool print_ir = true;
    bool function_pass_optimize = true;
};

class CodeGenerator {
public:
    explicit CodeGenerator(llvm::raw_ostream& os, CodeGeneratorSetting setting,
                           bool init = true);
    virtual ~CodeGenerator() = default;

    llvm::Value* codegen(std::unique_ptr<ArrayExpr> e);
    llvm::Value* codegen(Body b);
    llvm::Value* codegen(std::unique_ptr<ReturnExpr> e);
    llvm::Value* codegen(std::unique_ptr<VarDeclareExpr> e);
    llvm::Value* codegen(std::unique_ptr<UnaryExpr> e);
    llvm::Value* codegen(std::unique_ptr<ForExpr> e);
    llvm::Value* codegen(std::unique_ptr<IfExpr> e);
    llvm::Value* codegen(std::unique_ptr<CallExpr> e);
    llvm::Value* codegen(std::unique_ptr<VariableExpr> e);
    llvm::Value* codegen(std::unique_ptr<LiteralExpr> e);
    llvm::Value* codegen(std::unique_ptr<Expression> e);
    virtual llvm::Value* codegen(std::unique_ptr<BinaryExpr> e);

    llvm::Function* codegen(ProtoTypePtr p);
    llvm::Function* codegen(FunctionNode& f);

    virtual void codegen(std::vector<ASTNodePtr>&&);
    void print(std::string&& file_addr);

public:
    std::map<std::string, int> binary_oper_precedence_ = {
        {"=", 2}, {"<", 10}, {"+", 20}, {"-", 20}, {"*", 40}};
    std::unordered_map<std::string, TypeSystem::AggregateType> struct_table_;
    TypeManager type_manager_;
protected:
    llvm::Function* get_function(std::string& name);
    llvm::AllocaInst* create_entry_block_alloca(
        llvm::Function* function, const std::string& var_name, TypeSystem::TypeBase* type);
    llvm::AllocaInst* create_entry_block_alloca(
        llvm::Function* function, const std::string& var_name, llvm::Type* type);
protected:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::legacy::FunctionPassManager> function_pass_manager_;

    std::map<std::string, ProtoType> function_protos_ = {};
    std::string err_;
    llvm::ExitOnError exit_on_error_;
    llvm::raw_ostream& output_stream_;

    CodeGeneratorSetting setting_;
    OperatorFunctionManager operator_function_manager_;
    SymbolTable symbol_table_;

    llvm::BasicBlock* return_block = nullptr;
    llvm::AllocaInst* ret_alloca = nullptr;
};
