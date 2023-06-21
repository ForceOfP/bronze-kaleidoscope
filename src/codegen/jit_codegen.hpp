#pragma once

#include "ast/ast.hpp"
#include "codegen.hpp"
#include "jit_engine.hpp"
#include "extern.hpp"
#include <llvm/IR/Value.h>
#include <memory>

class JitCodeGenerator: public CodeGenerator {
public:
    explicit JitCodeGenerator(llvm::raw_ostream& os, CodeGeneratorSetting setting);

    void codegen(std::vector<ASTNodePtr>&& ast_tree) override;
    llvm::Value* codegen(std::unique_ptr<BinaryExpr> e) override;
    void initialize_llvm_elements();
private:
    std::unique_ptr<OrcJitEngine> jit_;    
};