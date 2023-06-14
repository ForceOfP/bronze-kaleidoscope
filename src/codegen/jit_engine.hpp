#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"

#include <memory>

class OrcJitEngine {
public:
    OrcJitEngine(
        std::unique_ptr<llvm::orc::ExecutionSession> es,
        llvm::orc::JITTargetMachineBuilder jtmb,
        llvm::DataLayout dl
    );

    ~OrcJitEngine();

    static llvm::Expected<std::unique_ptr<OrcJitEngine>> create() {
        auto epc = llvm::orc::SelfExecutorProcessControl::Create();
        if (!epc) return epc.takeError();
        auto es = std::make_unique<llvm::orc::ExecutionSession>(std::move(*epc));
        llvm::orc::JITTargetMachineBuilder jtmb(es->getExecutorProcessControl().getTargetTriple());
        auto dl = jtmb.getDefaultDataLayoutForTarget();
        if (!dl) return dl.takeError();
        return std::make_unique<OrcJitEngine>(std::move(es), std::move(jtmb), std::move(*dl));
    }

    const llvm::DataLayout& get_data_layout() const;
    llvm::orc::JITDylib& get_main_jit_dylib();

    llvm::Error add_module(llvm::orc::ThreadSafeModule tsm, llvm::orc::ResourceTrackerSP rt = nullptr);
    llvm::Expected<llvm::JITEvaluatedSymbol> lookup(llvm::StringRef name);
private:
    std::unique_ptr<llvm::orc::ExecutionSession> execution_session_;
    
    llvm::DataLayout layout_;
    llvm::orc::MangleAndInterner mangle_;
    llvm::orc::RTDyldObjectLinkingLayer object_layer_;
    llvm::orc::IRCompileLayer compiler_layer_;

    llvm::orc::JITDylib& main_jit_dylib_;
};