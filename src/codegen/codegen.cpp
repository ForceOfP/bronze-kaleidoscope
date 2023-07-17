#include "codegen.hpp"

#include <cstdint>
#include <iostream>
#include <llvm-15/llvm/IR/DerivedTypes.h>
#include <llvm-15/llvm/IR/Value.h>
#include <llvm/ADT/Optional.h>
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
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>
#include <cassert>

#include "ast/type.hpp"
#include "codegen/symbol_table.hpp"
#include "extern.hpp"
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

CodeGenerator::CodeGenerator(llvm::raw_ostream& os, CodeGeneratorSetting setting, bool init): output_stream_(os) {
    setting_.function_pass_optimize = setting.function_pass_optimize;
    setting_.print_ir = setting.print_ir;
    
    if (init) {
        context_ = std::make_unique<llvm::LLVMContext>();
        module_ = std::make_unique<llvm::Module>("my cool compiler", *context_);

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
}

llvm::AllocaInst* CodeGenerator::create_entry_block_alloca(
    llvm::Function* function, const std::string& var_name, TypeSystem::TypeBase* type) {
    llvm::IRBuilder<> builder(&function->getEntryBlock(), function->getEntryBlock().begin());
    auto llvm_type = type->llvm_type(*context_);
    return builder.CreateAlloca(llvm_type, nullptr, var_name);
}

llvm::AllocaInst* CodeGenerator::create_entry_block_alloca(llvm::Function* function,
                                                const std::string& var_name, llvm::Type* llvm_type) {
    llvm::IRBuilder<> builder(&function->getEntryBlock(), function->getEntryBlock().begin());
    return builder.CreateAlloca(llvm_type, nullptr, var_name);
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<ArrayExpr> e) {
    std::vector<llvm::Constant*> values;
    int cnt = 0;
    for (auto& element: e->elements) {
        auto element_value = codegen(std::move(element));
        if (!element_value) {
            err_ = "generate array value idx: " + std::to_string(cnt) + " error";
            return nullptr;
        }
        values.push_back(static_cast<llvm::Constant*>(element_value));
        cnt++;
    }
    auto array_type = TypeSystem::find_type_by_name(std::move(e->type), struct_table_);
    auto ans = array_type->get_llvm_value(*context_, std::any(values));
    if (!ans) {
        err_ = "Generate ConstantArray error";
        return nullptr;
    }
    return ans;
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<ReturnExpr> e) {
    llvm::Value* ret = codegen(std::move(e->ret));
    if (!err_.empty()) {
        return nullptr;
    }

    if (ret) {
        builder_->CreateStore(ret, ret_alloca);
    }

    builder_->CreateBr(return_block);
    return nullptr;
} 

llvm::Value* CodeGenerator::codegen(std::unique_ptr<VarDeclareExpr> e) {
    std::vector<llvm::AllocaInst*> old_bindings_{};
    llvm::Function* function = builder_->GetInsertBlock()->getParent();

    symbol_table_.step();

    auto& var_name = e->name;
    auto var_type = TypeSystem::find_type_by_name(std::move(e->type), struct_table_);
    auto init = std::move(e->value);

    llvm::Value* init_value = nullptr;
    if (init) {
        init_value = codegen(std::move(init));
        if (!err_.empty()) {
            return nullptr;
        }
    } else { // If not specified, use 0.0.
        init_value = var_type->llvm_init_value(*context_);
    }

    if (e->is_const) {
        if (!init_value) {
            output_stream_ << "error in store const\n";
        }
        if (var_type->is_aggregate() || var_type->is_data_structure()) {
            symbol_table_.add_constant(var_name, init_value, var_type->name());
        }
        symbol_table_.add_constant(var_name, init_value);
    } else {
        llvm::AllocaInst* alloca = nullptr;
        if (var_type->is_data_structure()) {            
            auto array_type = static_cast<TypeSystem::ArrayType*>(var_type.get());
            alloca = create_entry_block_alloca(function, var_name, array_type);
            
            auto shadow_global_array = new llvm::GlobalVariable(
                *module_, array_type->llvm_type(*context_), true,
                llvm::Function::InternalLinkage,
                static_cast<llvm::ConstantArray*>(init_value),
                "shadow"
            );

            builder_->CreateMemCpy(
                alloca, 
                alloca->getAlign(), 
                shadow_global_array,
                shadow_global_array->getAlign(),
                array_type->llvm_memory_size(*module_)
            );

            symbol_table_.add_variant(var_name, alloca, var_type->name());
        } else if (var_type->is_primitive()) {
            alloca = create_entry_block_alloca(function, var_name, var_type.get());
            builder_->CreateStore(init_value, alloca);
            symbol_table_.add_variant(var_name, alloca);
        } else if (var_type->is_aggregate()) {
            auto struct_type = static_cast<TypeSystem::AggregateType*>(var_type.get());
            alloca = create_entry_block_alloca(function, var_name, struct_type);

            symbol_table_.add_variant(var_name, alloca, var_type->name());            
        }
    }

    return nullptr;
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<UnaryExpr> e) {
    llvm::Value* opnd_value = codegen(std::move(e->operand));
    if (!opnd_value) {
        return nullptr;
    }

    if (!e->_operater.empty()) {
        llvm::Function* f = get_function(e->_operater);
        if (!f) {
            err_ = "Unknown unary operator";
            return nullptr;
        }
        return builder_->CreateCall(f, opnd_value, "unop");
    }
    return nullptr;
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
    auto double_type = std::make_unique<TypeSystem::DoubleType>();
    llvm::AllocaInst* alloca = create_entry_block_alloca(function, e->var_name, double_type.get());

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
    symbol_table_.step();
    symbol_table_.add_variant(e->var_name, alloca);

    // Emit the body of the loop.  This, like any other expr, can change the
    // current BB.  Note that we ignore the value computed by the body, but don't
    // allow an error.
    codegen(std::move(e->body));
    if (!err_.empty()) {
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
    symbol_table_.back();

    // for expr always returns 0.0.
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*context_));
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<IfExpr> e) {
    assert(!e->then.data.empty() && !e->_else.data.empty());
    
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

    llvm::Value* then_value = nullptr;
    
    then_value = codegen(std::move(e->then));
    if (!err_.empty()) {
        return nullptr;
    }
    
    if (then_value) {
        builder_->CreateBr(merge_block);
        then_block = builder_->GetInsertBlock();
    }

    // llvm implement a function named Function::insert() at llvm17...
    // but I'm using llvm15
    auto& else_inserting_blocks = function->getBasicBlockList();
    else_inserting_blocks.insert(function->end(), else_block);
    
    builder_->SetInsertPoint(else_block);

    llvm::Value* else_value = codegen(std::move(e->_else));
    if (!err_.empty()) {
        return nullptr;
    } 

    if (else_value) {
        builder_->CreateBr(merge_block);
        else_block = builder_->GetInsertBlock();
    }

    if (else_value && then_value && else_value->getType() == then_value->getType()) {
        auto& merge_inserting_blocks = function->getBasicBlockList();
        merge_inserting_blocks.insert(function->end(), merge_block);

        builder_->SetInsertPoint(merge_block);
        llvm::PHINode* phi = builder_->CreatePHI(else_value->getType(), 2, "iftmp");

        phi->addIncoming(then_value, then_block);
        phi->addIncoming(else_value, else_block);
        return phi;
    } else if (then_value || else_value) {
        err_ = "different return type in if";
        return nullptr;
    }

    return nullptr;
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

/*
this function should synchronize changed with JitCodeGenerator::codegen(std::unique_ptr<BinaryExpr> e) 
*/
llvm::Value* CodeGenerator::codegen(std::unique_ptr<BinaryExpr> e) {
    // Special case '=' because we don't want to emit the LHS as an expression.
    if (e->oper == "=") {
        auto lhse = static_cast<VariableExpr*>(e->lhs.get());
        if (!lhse) {
            err_ = "destination of '=' must be a variable";
            return nullptr;
        }

        llvm::Value* rhs_value = codegen(std::move(e->rhs));
        if (!err_.empty()) {
            return nullptr;
        }

        if (!lhse->addrs.empty()) { 
            if (lhse->is_array_offset) {
                std::vector<llvm::Value*> offset_values {llvm::ConstantInt::get(*context_, llvm::APInt(32, 0))};
                for (auto& index: lhse->addrs) {
                    auto& taked_index = std::get<ExpressionPtr>(index);
                    if (auto offset_value = codegen(std::move(taked_index))) {
                        offset_values.push_back(offset_value);
                    } else {
                        return nullptr;
                    }
                }

                if (!symbol_table_.store(builder_.get(), lhse->name, rhs_value, offset_values)) {
                    err_ = "SymbolTable store " + lhse->name + "failed.";
                    return nullptr;
                }                
            } else {
                std::vector<SymbolTable::ElementOrOffset> element_or_offsets;
                int index = 0;
                for (auto& addr: lhse->addrs) {
                    if (std::holds_alternative<ExpressionPtr>(addr)) {
                        auto& taked_index = std::get<ExpressionPtr>(addr);
                        if (auto offset_value = codegen(std::move(taked_index))) {
                            element_or_offsets.emplace_back(offset_value);
                        } else {
                            return nullptr;
                        }                        
                    } else {
                        element_or_offsets.emplace_back(std::get<std::string>(addr));
                    }
                }
                if (!symbol_table_.store(builder_.get(), lhse->name, rhs_value, element_or_offsets, struct_table_)) {
                    err_ = "SymbolTable store " + lhse->name + "failed.";
                    return nullptr;
                } 
            }
        } else {
            if (!symbol_table_.store(builder_.get(), lhse->name, rhs_value)) {
                err_ = "SymbolTable store " + lhse->name + "failed.";
                return nullptr;
            }            
        }

        return rhs_value; // support a = (b = c);
    }
    
    llvm::Value* l = codegen(std::move(e->lhs));
    llvm::Value* r = codegen(std::move(e->rhs));
    if (!r || !l) {
        return nullptr;
    }

    auto type = r->getType();
    if (type != l->getType()) {
        err_ = "binary operator with two different type, left is " 
                + std::string(l->getName()) 
                + ", right is "
                + std::string(r->getName());
        return nullptr;
    }

    if (!operator_function_manager_.exist(type, e->oper)) {
        llvm::Function* function_value = get_function(e->oper);
        assert(function_value && "binary operator not found!");
        sleep(1);
        operator_function_manager_.add_function(type, e->oper, function_value);
    }

    auto f = operator_function_manager_.get_function(type, e->oper);
    return f(builder_.get(), l, r);
}

// def f() -> double {var x:array%array%double%2%2 = [[2, 2]:double, [3, 3]:double]:double; x[0][1] = 4; return x[0][1] + x[1][0];}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<VariableExpr> e) {
    if (auto ret = symbol_table_.load(builder_.get(), e->name)) {
        if (!e->addrs.empty()) {
            if (e->is_array_offset) {
                std::vector<llvm::Value*> offset_values {llvm::ConstantInt::get(*context_, llvm::APInt(32, 0))};
                
                for (auto& offset: e->addrs) {
                    auto& taked_offset = std::get<ExpressionPtr>(offset);
                    auto offset_value = codegen(std::move(taked_offset));
                    if (!offset_value) {
                        return nullptr;
                    }
                    offset_values.push_back(offset_value);
                }
                auto target_type = static_cast<llvm::AllocaInst*>(ret)->getAllocatedType();
                auto target_ptr = builder_->CreateInBoundsGEP(target_type, ret, offset_values, "offset");
                for (int i = 0; i < offset_values.size() - 1; i++) {
                    target_type = static_cast<llvm::ArrayType*>(target_type)->getArrayElementType();
                }
                ret = builder_->CreateLoad(target_type, target_ptr, "offsetValue");
            } else {
                auto target_llvm_type = static_cast<llvm::AllocaInst*>(ret)->getAllocatedType();
                llvm::Value* target_ptr = nullptr;
                auto target_front_end_type_str = symbol_table_.find_symbol_type_str(e->name);
                for (auto& addr: e->addrs) {
                    std::vector<llvm::Value*> offset_values {llvm::ConstantInt::get(*context_, llvm::APInt(32, 0))};
                    if (std::holds_alternative<ExpressionPtr>(addr)) {
                        auto taked_offset = std::move(std::get<ExpressionPtr>(addr));
                        auto offset_value = codegen(std::move(taked_offset));
                        if (!offset_value) {
                            return nullptr;
                        }
                        offset_values.push_back(offset_value);
                        target_ptr = builder_->CreateInBoundsGEP(target_llvm_type, ret, offset_values, "offset");
                        target_llvm_type = static_cast<llvm::ArrayType*>(target_llvm_type)->getArrayElementType();
                        target_front_end_type_str = TypeSystem::extract_nesting_type(target_front_end_type_str).first;
                    } else {
                        auto& target_front_end_type = struct_table_.at(target_front_end_type_str);
                        auto& taked_element_name = std::get<std::string>(addr);
                        unsigned int index = target_front_end_type.element_position(taked_element_name);

                        offset_values.push_back(llvm::ConstantInt::get(*context_, llvm::APInt(32, index)));
                        target_ptr = builder_->CreateInBoundsGEP(target_llvm_type, ret, offset_values, "offset");
                        target_llvm_type = static_cast<llvm::StructType*>(target_llvm_type)->getStructElementType(index);
                    }
                }
                ret = builder_->CreateLoad(target_llvm_type, target_ptr, "offsetValue");
            }
        }
        return ret;
    }

    err_ = "SymbolTable load " + e->name + " failed.";
    return nullptr;
}

llvm::Value* CodeGenerator::codegen(std::unique_ptr<LiteralExpr> e) {
    auto tmp = e->type;
    auto literal_type = TypeSystem::find_type_by_name(std::move(tmp), struct_table_);
    return literal_type->get_llvm_value(*context_, e->value);
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

    auto var = dynamic_cast<VarDeclareExpr*>(raw);
    if (var) {
        std::unique_ptr<VarDeclareExpr> tmp(var);
        return codegen(std::move(tmp));  
    }

    auto r = dynamic_cast<ReturnExpr*>(raw);
    if (r) {
        std::unique_ptr<ReturnExpr> tmp(r);
        return codegen(std::move(tmp));  
    }

    auto arr = dynamic_cast<ArrayExpr*>(raw);
    if (arr) {
        std::unique_ptr<ArrayExpr> tmp(arr);
        return codegen(std::move(tmp));          
    }

    return nullptr;
}

llvm::Value* CodeGenerator::codegen(Body b) {
    llvm::Value* tmp = nullptr;

    for (auto& expr: b.data) {
        tmp = codegen(std::move(expr));
        if (!err_.empty()) {
            return nullptr;
        }
    }
    if (b.has_return_value) {
        return tmp;
    } else {
        return nullptr;
    }
}

llvm::Function* CodeGenerator::codegen(ProtoTypePtr p) {
    assert(p);

    std::vector<llvm::Type*> arg_types;
    for (auto& arg: p->args) {
        auto arg_type = TypeSystem::find_type_by_name(std::move(arg.second), struct_table_);
        arg_types.push_back(arg_type->llvm_type(*context_));
    }
    auto answer_type = TypeSystem::find_type_by_name(std::move(p->answer), struct_table_);
    llvm::Type* result_type = answer_type->llvm_type(*context_);
    llvm::FunctionType* function_type = llvm::FunctionType::get(result_type, arg_types, false);
    llvm::Function* function = llvm::Function::Create(
        function_type, 
        llvm::Function::ExternalLinkage,
        p->name,
        module_.get()
    );

    unsigned idx = 0;
    assert(function->arg_size() == p->args.size());
    for (auto& arg: function->args()) {
        arg.setName(p->args[idx++].first);
    }
    return function;
}

llvm::Function* CodeGenerator::codegen(FunctionNode& f) {
    // Transfer ownership of the prototype to the FunctionProtos map

    auto name = f.prototype->name;
    function_protos_[name] = std::move(*f.prototype);
    auto function = get_function(name);

    if (!function) {
        return nullptr;
    }

    llvm::BasicBlock* basic_block = llvm::BasicBlock::Create(*context_, "entry", function);
    builder_->SetInsertPoint(basic_block);

    // Record the function arguments in the NamedValues map.
    // named_values_alloca_.clear();
    symbol_table_.step();
    for (auto& arg: function->args()) {
        // Create an alloca for this variable.
        auto alloca = create_entry_block_alloca(function, std::string(arg.getName()), arg.getType());

        // Store the initial value into the alloca.
        builder_->CreateStore(&arg, alloca);

        symbol_table_.add_variant(std::string(arg.getName()), alloca);
    }

    return_block = llvm::BasicBlock::Create(*context_, "return");
    ret_alloca = create_entry_block_alloca(function, "retAlloca", function->getReturnType());
    auto& return_inserting_blocks = function->getBasicBlockList();
    return_inserting_blocks.insert(function->end(), return_block); 

    codegen(std::move(f.body));
    if (err_.empty()) {
        builder_->SetInsertPoint(return_block);
        llvm::Value* ret_value = builder_->CreateLoad(ret_alloca->getAllocatedType(), ret_alloca, "final");
        builder_->CreateRet(ret_value);
        
        // Validate the generated code, checking for consistency.
        llvm::verifyFunction(*function);

        if (setting_.function_pass_optimize) {
            // function_pass_manager_->run(*function);
        }
        
        symbol_table_.back();
        return function;
    }

    symbol_table_.back();
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
                    output_stream_ << "shouldn't use exec without jit.\n";
                } else {
                    if (auto ir = codegen(f)) {
                        if (setting_.print_ir) {
                            ir->print(output_stream_);
                        } else {
                            output_stream_ << "parsed function definition.\n";
                        }
                    } else {
                        output_stream_ << err_ << '\n';
                    }                        
                }
            },
            [&](StructNode& s) {
                //struct_table_[s.name] = std::make_unique<TypeSystem::AggregateType>(s.name, s.elements);
                struct_table_.insert({s.name, TypeSystem::AggregateType(s.name, s.elements, struct_table_)});
                output_stream_ << "parsed struct definition.\n";
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
