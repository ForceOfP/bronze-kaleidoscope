#include "codegen.hpp"

#include <cassert>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
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
#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <vector>

#include "ast/ast.hpp"
#include "codegen/jit_engine.hpp"

void CodeGenerator::initialize_llvm_elements() {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>("my cool jit", *context_);
    module_->setDataLayout(jit_->get_data_layout());

    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);    
    function_pass_manager_ = std::make_unique<llvm::legacy::FunctionPassManager>(module_.get());

    function_pass_manager_->add(llvm::createInstructionCombiningPass());
    function_pass_manager_->add(llvm::createReassociatePass());
    function_pass_manager_->add(llvm::createGVNPass());
    function_pass_manager_->add(llvm::createCFGSimplificationPass());

    function_pass_manager_->doInitialization();
}

CodeGenerator::CodeGenerator(llvm::raw_ostream& os): output_stream_(os) {
    exit_on_error_ = llvm::ExitOnError();
    jit_ = exit_on_error_(OrcJitEngine::create());
    
    initialize_llvm_elements();
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<CallExpr> e) {
    //llvm::Function* callee_func = module_->getFunction(e->callee);
    auto callee_func = get_function(e->callee);

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
    assert(p);

    // all types are double, like double(double, double) etc.a
    std::vector<llvm::Type*> arg_types(p->args.size(), llvm::Type::getDoubleTy(*context_));
    llvm::FunctionType* function_type = llvm::FunctionType::get(llvm::Type::getDoubleTy(*context_), arg_types, false);
    llvm::Function* function = llvm::Function::Create(
        function_type, 
        llvm::Function::ExternalLinkage,
        p->name,
        module_.get()
    );

    unsigned idx = 0;
    assert(function->arg_size() == p->args.size());
    for (auto& arg: function->args()) {
        arg.setName(p->args[idx++]);
    }
    return function;
}

llvm::Function* CodeGenerator::codegen(FunctionNode& f) {
    // Transfer ownership of the prototype to the FunctionProtos map

    auto name = f.prototype->name;
    function_protos_[name] = *f.prototype;
    auto function = get_function(name);

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

        function_pass_manager_->run(*function);
        
        return function;
    }

    function->eraseFromParent();
    return nullptr;
}

void CodeGenerator::codegen(std::vector<ASTNodePtr>&& ast_tree) {
    for (auto& ast: ast_tree) {
        ast->match(
            [&](ExternNode& e) {
                std::string name = e.prototype->name;
                function_protos_[name] = *e.prototype;
                if (auto ir = codegen(std::move(e.prototype))) {
                    ir->print(output_stream_);
                } else {
                    output_stream_ << err_ << '\n';
                    function_protos_.erase(name);
                }
            },
            [&](FunctionNode& f) {
                bool is_top = f.prototype->name == "__anon_expr";
                if (is_top) {
                    if (auto ir = codegen(f)) {
                        auto rt = jit_->get_main_jit_dylib().createResourceTracker();
                        auto tsm = llvm::orc::ThreadSafeModule(
                            std::move(module_),
                          std::move(context_)
                        );
                        exit_on_error_(jit_->add_module(std::move(tsm), rt));
                        initialize_llvm_elements();

                        auto expr_symbol = exit_on_error_(jit_->lookup("__anon_expr"));
                        auto address = expr_symbol.getAddress();
                        auto fp = llvm::jitTargetAddressToPointer<double (*)()>(address);
                        output_stream_ << fp() << '\n';
                        exit_on_error_(rt->remove());
                    } else {
                        output_stream_ << err_ << '\n';
                    }
                } else {
                    if (auto ir = codegen(f)) {
                        ir->print(output_stream_);
                        exit_on_error_(jit_->add_module(
                            llvm::orc::ThreadSafeModule(std::move(module_),std::move(context_))
                        ));
                        initialize_llvm_elements();
                    } else {
                        output_stream_ << err_ << '\n';
                    }
                }
            }
        );
    }
}

llvm::Function* CodeGenerator::get_function(std::string name) {
    if (auto f = module_->getFunction(name)) {
        return f;
    }

    auto iter = function_protos_.find(name);
    if (iter != function_protos_.end()) {
        auto proto = iter->second;
        auto ans = codegen(std::make_unique<ProtoType>(proto));
        return ans;
    }

    return nullptr;
}


#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
    fputc((char)X, stderr);
    return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
    fprintf(stderr, "%f\n", X);
    return 0;
}
