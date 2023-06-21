#include "jit_codegen.hpp"
#include "codegen/codegen.hpp"
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>

JitCodeGenerator::JitCodeGenerator(llvm::raw_ostream& os, CodeGeneratorSetting setting): CodeGenerator(os, setting, false) {
    exit_on_error_ = llvm::ExitOnError();
    jit_ = exit_on_error_(OrcJitEngine::create());
    initialize_llvm_elements();
}

void JitCodeGenerator::initialize_llvm_elements() {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>("my cool jit", *context_);
    module_->setDataLayout(jit_->get_data_layout());

    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);    
    if (setting_.function_pass_optimize) {
        function_pass_manager_ = std::make_unique<llvm::legacy::FunctionPassManager>(module_.get());

        function_pass_manager_->add(llvm::createPromoteMemoryToRegisterPass());
        function_pass_manager_->add(llvm::createInstructionCombiningPass());
        function_pass_manager_->add(llvm::createReassociatePass());
        function_pass_manager_->add(llvm::createGVNPass());
        function_pass_manager_->add(llvm::createCFGSimplificationPass());
    
        function_pass_manager_->doInitialization();
    }
}

void JitCodeGenerator::codegen(std::vector<ASTNodePtr>&& ast_tree) {
    for (auto& ast: ast_tree) {
        ast->match(
            [&](ExternNode& e) {
                std::string name = e.prototype->name;
                function_protos_[name] = *e.prototype;
                if (auto ir = CodeGenerator::codegen(std::move(e.prototype))) {
                    ir->print(output_stream_);
                } else {
                    output_stream_ << err_ << '\n';
                    function_protos_.erase(name);
                }
            },
            [&](FunctionNode& f) {
                bool is_top = f.prototype->name == "__anon_expr";
                if (is_top) {
                    if (auto ir = CodeGenerator::codegen(f)) {
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
                        
                        output_stream_ << std::to_string(fp()) << '\n';
                        exit_on_error_(rt->remove());
                    } else {
                        output_stream_ << err_ << '\n';
                    }
                } else {
                    if (auto ir = CodeGenerator::codegen(f)) {
                        if (setting_.print_ir) {
                            ir->print(output_stream_);
                        } else {
                            output_stream_ << "parsed definition.\n";
                        }
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

llvm::Value* JitCodeGenerator::codegen(std::unique_ptr<BinaryExpr> e) {
    // Special case '=' because we don't want to emit the LHS as an expression.
    if (e->oper == "=") {
        auto lhse = static_cast<VariableExpr*>(e->lhs.get());
        if (!lhse) {
            err_ = "destination of '=' must be a variable";
            return nullptr;
        }

        llvm::Value* rhs_value = CodeGenerator::codegen(std::move(e->rhs));
        if (!rhs_value) {
            return nullptr;
        }

        llvm::Value* lhs_value_alloca = named_values_alloca_[lhse->name]; 
        if (!lhs_value_alloca) {
            err_ = "Unknown variable name";
            return nullptr;
        }

        builder_->CreateStore(rhs_value, lhs_value_alloca);
        return rhs_value; // support a = (b = c);
    }
    
    llvm::Value* l = CodeGenerator::codegen(std::move(e->lhs));
    llvm::Value* r = CodeGenerator::codegen(std::move(e->rhs));
    if (!r || !l) {
        return nullptr;
    }

    auto type = r->getType();
    if (type != l->getType()) {
        err_ = "binary operator with two different type";
        return nullptr;
    }

    if (operator_function_manager_.exist(type, e->oper)) {
        auto f = operator_function_manager_.get_function(r->getType(), e->oper);
        return f(builder_.get(), l, r);
    }
    llvm::Function* f = get_function(e->oper);
    assert(f && "binary operator not found!");
    llvm::Value* ops[2] = {l, r};
    return builder_->CreateCall(f, ops, "binop");
}