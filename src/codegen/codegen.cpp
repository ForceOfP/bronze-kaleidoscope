#include "codegen.hpp"
#include "ast/ast.hpp"
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <iostream>
#include <llvm-15/llvm/Support/raw_ostream.h>
#include <memory>
#include <vector>

CodeGenerator::CodeGenerator(std::vector<ASTNodePtr>&& tree): ast_tree_(std::move(tree)) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>("my cool jit", *context_);
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);    
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<CallExpr> e) {
    llvm::Function* callee_func = module_->getFunction(e->callee);
    if (!callee_func) {
        err_ = "Unknown function referenced";
        return nullptr;
    }

    if (callee_func->arg_size() != e->args.size()) {
        err_ = "Incorrect # arguments passed";
        return nullptr;
    }

    std::vector<llvm::Value*> args_values{};
    for (auto & arg : e->args) {
        args_values.push_back(codegen(std::move(arg)));
        if (!args_values.back()) {
            return nullptr;
        }
    }

    return builder_->CreateCall(callee_func, args_values, "calltmp");
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<BinaryExpr> e) {
    llvm::Value* l = codegen(std::move(e->lhs));
    llvm::Value* r = codegen(std::move(e->rhs));
    if (!r || !l) {
        return nullptr;
    }

    if (e->oper == "+") {
        return builder_->CreateFAdd(l, r, "addtmp");
    }
    if (e->oper == "-") {
        return builder_->CreateFSub(l, r, "subtmp");
    }
    if (e->oper == "*") {
        return builder_->CreateFMul(l, r, "multmp");
    }
    if (e->oper == "<") {
        l = builder_->CreateFCmpULT(l, r);
        return builder_->CreateUIToFP(l, llvm::Type::getDoubleTy(*context_), "booltmp");
    }

    err_ = "invalid binary operator";
    return nullptr;
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<VariableExpr> e) {
    if (!named_values_.count(e->name)) {
        err_ = "Unknown variable name";
        return nullptr;
    }
    return named_values_[e->name];
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<LiteralExpr> e) {
    return llvm::ConstantFP::get(*context_, llvm::APFloat(e->value));
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<Expression> e) {
    Expression* raw = e.release();
    auto l = dynamic_cast<LiteralExpr*>(raw);
    if (l) {
        std::unique_ptr<LiteralExpr> tmp(l);
        return codegen(std::move(tmp));
    }

    auto c = dynamic_cast<CallExpr*>(raw);
    if (c) {
        std::unique_ptr<CallExpr> tmp(c);
        return codegen(std::move(tmp));
    }
    
    auto v = dynamic_cast<VariableExpr*>(raw);
    if (v) {
        std::unique_ptr<VariableExpr> tmp(v);
        return codegen(std::move(tmp));
    }
    
    auto b = dynamic_cast<BinaryExpr*>(raw);
    if (b) {
        std::unique_ptr<BinaryExpr> tmp(b);
        return codegen(std::move(tmp));
    }

    return nullptr;
}

llvm::Function* CodeGenerator::codegen(ProtoTypePtr p) {
    // all types are double, like double(double, double) etc.
    std::vector<llvm::Type*> arg_types(p->args.size(), llvm::Type::getDoubleTy(*context_));
    llvm::FunctionType* function_type = llvm::FunctionType::get(llvm::Type::getDoubleTy(*context_), arg_types, false);
    llvm::Function* function = llvm::Function::Create(
        function_type, 
        llvm::Function::ExternalLinkage,
        p->name,
        module_.get()
    );

    unsigned idx = 0;
    for (auto& arg: function->args()) {
        arg.setName(p->args[idx++]);
    }
    return function;
}

llvm::Function* CodeGenerator::codegen(FunctionNode& f) {
    // First, check for an existing function from a previous 'extern' declaration.
    llvm::Function* function = module_->getFunction(f.prototype->name);

    if (!function) {
        function = codegen(std::move(f.prototype));
    }

    if (!function) {
        return nullptr;
    }

    llvm::BasicBlock* basic_block = llvm::BasicBlock::Create(*context_, "entry", function);
    builder_->SetInsertPoint(basic_block);

    named_values_.clear();
    for (auto& arg: function->args()) {
        named_values_.insert({std::string(arg.getName()), &arg});
    }

    if (llvm::Value* return_value = codegen(std::move(f.body))) {
        builder_->CreateRet(return_value);
        
        // Validate the generated code, checking for consistency.
        llvm::verifyFunction(*function);
        
        return function;
    }

    function->eraseFromParent();
    return nullptr;
}

void CodeGenerator::codegen(llvm::raw_ostream& output_stream) {
    for (auto& ast: ast_tree_) {
        ast->match(
            [&](FunctionNode& f) {
                bool flag = f.prototype->name == "__anon_expr";

                if (auto ir = codegen(f)) {
                    ir->print(output_stream);
                    if (flag) ir->removeFromParent();
                }
            }, 
            [&](ExternNode& e) {
                if (auto ir = codegen(std::move(e.prototype))) {
                    ir->print(output_stream);
                }
            }  
        );
    }
}