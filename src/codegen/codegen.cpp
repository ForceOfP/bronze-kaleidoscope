#include "codegen.hpp"

#include <llvm/IR/Type.h>
#include <llvm/ADT/Optional.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm-c/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <cassert>

#include "ast/ast.hpp"
#include "codegen/jit_engine.hpp"

void CodeGenerator::print(std::string&& file_addr) {
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    auto target_triple = LLVMGetDefaultTargetTriple();
    module_->setTargetTriple(target_triple);

    std::string target_error;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, target_error);
    if (!target) {
        output_stream_ << target_error << '\n';
        exit(1);
    }

    auto cpu = "generic";
    auto features = "";

    llvm::TargetOptions opt;
    auto rm = llvm::Optional<llvm::Reloc::Model>();

    auto target_machine = target->createTargetMachine(
        target_triple, cpu, features, opt, rm);

    module_->setDataLayout(target_machine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(file_addr, ec, llvm::sys::fs::OF_None);

    if (ec) {
        output_stream_ << "Could not open file: " << ec.message() << '\n';\
        exit(1);
    }

    llvm::legacy::PassManager pass;
    auto file_type = llvm::CGFT_ObjectFile;

    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        output_stream_ << "TargetMachine can't emit a file of this type\n";
        exit(1);
    }

    pass.run(*module_);
    dest.flush();

    output_stream_ << "Wrote " << file_addr << "\n";
}

void CodeGenerator::initialize_llvm_elements() {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>("my cool jit", *context_);
    if (setting_.use_jit_else_compile) {
        module_->setDataLayout(jit_->get_data_layout());
    }

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

CodeGenerator::CodeGenerator(llvm::raw_ostream& os, bool use_jit): output_stream_(os) {
    setting_.use_jit_else_compile = use_jit;
    exit_on_error_ = llvm::ExitOnError();
    if (setting_.use_jit_else_compile) {
        jit_ = exit_on_error_(OrcJitEngine::create());
    }

    initialize_llvm_elements();
}

llvm::AllocaInst* CodeGenerator::create_entry_block_alloca(llvm::Function* function, const std::string& var_name) {
    llvm::IRBuilder<> builder(&function->getEntryBlock(), function->getEntryBlock().begin());
    return builder.CreateAlloca(llvm::Type::getDoubleTy(*context_), nullptr, var_name);
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<VarExpr> e) {
    std::vector<llvm::AllocaInst*> old_bindings_{};
    llvm::Function* function = builder_->GetInsertBlock()->getParent();

    // Register all variables and emit their initializer.
    for (auto i = 0; i < e->var_names.size(); i++) {
        const std::string& var_name = e->var_names[i].first;
        auto init = std::move(e->var_names[i].second);

        // Emit the initializer before adding the variable to scope, this prevents
        // the initializer from referencing the variable itself, and permits stuff
        // like this:
        //  var a = 1 in
        //    var a = a in ...   # refers to outer 'a'.
        llvm::Value* init_value;
        if (init) {
            init_value = codegen(std::move(init));
            if (!init_value) {
                return nullptr;
            }
        } else { // If not specified, use 0.0.
            init_value = llvm::ConstantFP::get(*context_, llvm::APFloat(0.0));
        }

        llvm::AllocaInst* alloca = create_entry_block_alloca(function, var_name);
        builder_->CreateStore(init_value, alloca);

        // Remember the old variable binding so that we can restore the binding when
        // we unrecurse.
        old_bindings_.push_back(named_values_alloca_[var_name]);

        named_values_alloca_[var_name] = alloca;
    }

    // Codegen the body, now that all vars are in scope.
    llvm::Value* body_val = codegen(std::move(e->body));
    if (!body_val) {
        return nullptr;
    }

    // Pop all our variables from scope.
    for (auto i = 0; i < e->var_names.size(); i++) {
        named_values_alloca_[e->var_names[i].first] = old_bindings_[i];
    }

    // Return the body computation.
    return body_val;
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<UnaryExpr> e) {
    llvm::Value* opnd_value = codegen(std::move(e->operand));
    if (!opnd_value) {
        return nullptr;
    }

    llvm::Function* f = get_function(e->_operater);
    if (!f) {
        err_ = "Unknown unary operator";
        return nullptr;
    }

    return builder_->CreateCall(f, opnd_value, "unop");
}

// Output for-loop as:
//   ...
//   start = startexpr
//   goto loop
// loop:
//   variable = phi [start, loopheader], [nextvariable, loopend]
//   ...
//   bodyexpr
//   ...
// loopend:
//   step = stepexpr
//   nextvariable = variable + step
//   endcond = endexpr
//   br endcond, loop, endloop
// outloop:
llvm::Value* CodeGenerator::codegen(std::unique_ptr<ForExpr> e) {
    llvm::Function* function = builder_->GetInsertBlock()->getParent();
    llvm::AllocaInst* alloca = create_entry_block_alloca(function, e->var_name);

    auto start_value = codegen(std::move(e->start));
    if (!start_value) {
        return nullptr;
    }
    builder_->CreateStore(start_value, alloca);

    // llvm::BasicBlock* pre_header_block = builder_->GetInsertBlock();
    llvm::BasicBlock* loop_block = llvm::BasicBlock::Create(*context_, "loop", function);

    builder_->CreateBr(loop_block);
    builder_->SetInsertPoint(loop_block);
    
    // Start the PHI node with an entry for Start.
    // llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getDoubleTy(*context_), 2, e->var_name);
    // phi->addIncoming(start_value, pre_header_block);

    // Within the loop, the variable is defined equal to the PHI node.  If it
    // shadows an existing variable, we have to restore it, so save it now.
    auto old = named_values_alloca_[e->var_name];
    named_values_alloca_[e->var_name] = alloca;

    // Emit the body of the loop.  This, like any other expr, can change the
    // current BB.  Note that we ignore the value computed by the body, but don't
    // allow an error.
    if (!codegen(std::move(e->body))) {
        return nullptr;
    }

    llvm::Value* step_value = nullptr;
    if (e->step) {
        step_value = codegen(std::move(e->step));
        if (!step_value) {
            return nullptr;
        }
    } else {
        step_value = llvm::ConstantFP::get(*context_, llvm::APFloat(1.0)); // default step is 1.0
    }

    // llvm::Value* next_var = builder_->CreateFAdd(phi, step_value, "nextvar");
    auto end_condition = codegen(std::move(e->end));
    if (!end_condition) {
        return nullptr;
    }

    // Reload, increment, and restore the alloca.  This handles the case where
    // the body of the loop mutates the variable.
    llvm::Value* now_var = builder_->CreateLoad(alloca->getAllocatedType(), alloca, std::string(e->var_name));
    llvm::Value* next_var = builder_->CreateFAdd(now_var, step_value, "nextvar");
    builder_->CreateStore(next_var, alloca);

    end_condition = builder_->CreateFCmpONE(
        end_condition, llvm::ConstantFP::get(*context_, llvm::APFloat(0.0)), "loopcond");
    
    // llvm::BasicBlock* loop_end_block = builder_->GetInsertBlock();
    llvm::BasicBlock* after_block = llvm::BasicBlock::Create(*context_, "afterloop", function);

    builder_->CreateCondBr(end_condition, loop_block, after_block);
    builder_->SetInsertPoint(after_block);

    // phi->addIncoming(next_var, loop_end_block);
    if (old) {
        named_values_alloca_[e->var_name] = old;
    } else {
        named_values_alloca_.erase(e->var_name);
    }

    // for expr always returns 0.0.
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*context_));
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<IfExpr> e) {
    auto cond_value = codegen(std::move(e->condition));
    if (!cond_value) {
        return nullptr;
    }

    cond_value = builder_->CreateFCmpONE(
        cond_value, llvm::ConstantFP::get(*context_, llvm::APFloat(0.0)), "ifcond");
    
    llvm::Function* function = builder_->GetInsertBlock()->getParent();

    llvm::BasicBlock* then_block = llvm::BasicBlock::Create(*context_, "then", function);
    llvm::BasicBlock* else_block = llvm::BasicBlock::Create(*context_, "else");
    llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(*context_, "ifcont");

    builder_->CreateCondBr(cond_value, then_block, else_block);
    builder_->SetInsertPoint(then_block);

    llvm::Value* then_value = codegen(std::move(e->then));
    if (!then_value) {
        return nullptr;
    }

    builder_->CreateBr(merge_block);
    then_block = builder_->GetInsertBlock();

    // llvm implement a function named Function::insert() at llvm17...
    // but I'm using llvm15
    auto& else_inserting_blocks = function->getBasicBlockList();
    else_inserting_blocks.insert(function->end(), else_block);
    
    builder_->SetInsertPoint(else_block);

    llvm::Value* else_value = codegen(std::move(e->_else));
    if (!else_value) {
        return nullptr;
    } 

    builder_->CreateBr(merge_block);
    else_block = builder_->GetInsertBlock();

    // like behind.
    auto& merge_inserting_blocks = function->getBasicBlockList();
    merge_inserting_blocks.insert(function->end(), merge_block);

    builder_->SetInsertPoint(merge_block);
    llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getDoubleTy(*context_), 2, "iftmp");

    phi->addIncoming(then_value, then_block);
    phi->addIncoming(else_value, else_block);
    return phi;
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
    // Special case '=' because we don't want to emit the LHS as an expression.
    if (e->oper == "=") {
        auto lhse = static_cast<VariableExpr*>(e->lhs.get());
        if (!lhse) {
            err_ = "destination of '=' must be a variable";
            return nullptr;
        }

        llvm::Value* rhs_value = codegen(std::move(e->rhs));
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
    
    llvm::Value* l = codegen(std::move(e->lhs));
    llvm::Value* r = codegen(std::move(e->rhs));
    if (!r || !l) {
        return nullptr;
    }

    auto type = r->getType();
    if (type != l->getType()) {
        err_ = "binary operator with two different type";
        return nullptr;
    }

    if (setting_.use_jit_else_compile) {
        if (operator_function_manager_.exist(type, e->oper)) {
            auto f = operator_function_manager_.get_function(r->getType(), e->oper);
            return f(builder_.get(), l, r);
        }
        llvm::Function* f = get_function(e->oper);
        assert(f && "binary operator not found!");
        llvm::Value* ops[2] = {l, r};
        return builder_->CreateCall(f, ops, "binop");
    } else {
        if (!operator_function_manager_.exist(type, e->oper)) {
            llvm::Function* function_value = get_function(e->oper);
            assert(function_value && "binary operator not found!");
            sleep(1);
            operator_function_manager_.add_function(type, e->oper, function_value);
        }

        auto f = operator_function_manager_.get_function(type, e->oper);
        return f(builder_.get(), l, r);
    }
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<VariableExpr> e) {
    auto a = named_values_alloca_[e->name];
    if (!a) {
        err_ = "Unknown variable name";
        return nullptr;
    }
    return builder_->CreateLoad(a->getAllocatedType(), a, e->name.c_str());
    // return named_values_[e->name];
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

    auto i = dynamic_cast<IfExpr*>(raw);
    if (i) {
        std::unique_ptr<IfExpr> tmp(i);
        return codegen(std::move(tmp));        
    }

    auto f = dynamic_cast<ForExpr*>(raw);
    if (f) {
        std::unique_ptr<ForExpr> tmp(f);
        return codegen(std::move(tmp));
    }

    auto u = dynamic_cast<UnaryExpr*>(raw);
    if (u) {
        std::unique_ptr<UnaryExpr> tmp(u);
        return codegen(std::move(tmp));        
    }

    auto var = dynamic_cast<VarExpr*>(raw);
    if (var) {
        std::unique_ptr<VarExpr> tmp(var);
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

/*     if (f.prototype->is_binary_oper()) {
        binary_oper_precedence_[f.prototype->get_operator_name()] = f.prototype->get_binary_precedence();
    } */

    llvm::BasicBlock* basic_block = llvm::BasicBlock::Create(*context_, "entry", function);
    builder_->SetInsertPoint(basic_block);

    // Record the function arguments in the NamedValues map.
    named_values_alloca_.clear();
    for (auto& arg: function->args()) {
        // Create an alloca for this variable.
        auto alloca = create_entry_block_alloca(function, std::string(arg.getName()));

        // Store the initial value into the alloca.
        builder_->CreateStore(&arg, alloca);

        named_values_alloca_.insert({std::string(arg.getName()), alloca});
    }

    if (llvm::Value* return_value = codegen(std::move(f.body))) {
        builder_->CreateRet(return_value);
        
        // Validate the generated code, checking for consistency.
        llvm::verifyFunction(*function);

        if (setting_.function_pass_optimize) {
            function_pass_manager_->run(*function);
        }
        
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
                    if (setting_.use_jit_else_compile) {
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
                            
                            output_stream_ << std::to_string(fp()) << '\n';
                            exit_on_error_(rt->remove());
                        } else {
                            output_stream_ << err_ << '\n';
                        }
                    } else {
                        if (auto ir = codegen(f)) {
                            ir->print(output_stream_);
                        } else {
                            output_stream_ << err_ << '\n';
                        }
                    }
                } else {
                    if (setting_.use_jit_else_compile) {
                        if (auto ir = codegen(f)) {
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
                    } else {
                        if (auto ir = codegen(f)) {
                            if (setting_.print_ir) {
                                ir->print(output_stream_);
                            }/*  else {
                                output_stream_ << "parsed definition.\n";
                            } */
                        } else {
                            output_stream_ << err_ << '\n';
                        }                        
                    }
                }
            }
        );
    }
}

llvm::Function* CodeGenerator::get_function(std::string& name) {
    if (auto f = module_->getFunction(name)) {
        return f;
    }

    auto iter = function_protos_.find(name);
    if (iter != function_protos_.end()) {
        auto proto = iter->second;
        auto ans = codegen(std::make_unique<ProtoType>(proto));
        return ans;
    }

    output_stream_ << "not find function!\n";
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
