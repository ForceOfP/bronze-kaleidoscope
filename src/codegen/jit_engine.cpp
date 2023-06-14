#include "jit_engine.hpp"
#include <memory>

OrcJitEngine::OrcJitEngine(
    std::unique_ptr<llvm::orc::ExecutionSession> es,
    llvm::orc::JITTargetMachineBuilder jtmb,
    llvm::DataLayout dl
):  execution_session_(std::move(es)), 
    layout_(std::move(dl)), 
    mangle_(*this->execution_session_, this->layout_),
    object_layer_(*this->execution_session_, 
                    []() {return std::make_unique<llvm::SectionMemoryManager>();}),
    compiler_layer_(*this->execution_session_, 
                    object_layer_,
                    std::make_unique<llvm::orc::ConcurrentIRCompiler>(std::move(jtmb))),
    main_jit_dylib_(this->execution_session_->createBareJITDylib("<main>")) {
    main_jit_dylib_.addGenerator(
        llvm::cantFail(
            llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(layout_.getGlobalPrefix())
        )
    );
    if (jtmb.getTargetTriple().isOSBinFormatCOFF()) {
        object_layer_.setOverrideObjectFlagsWithResponsibilityFlags(true);
        object_layer_.setAutoClaimResponsibilityForObjectSymbols(true);
    }
}

OrcJitEngine::~OrcJitEngine() {
    if (auto err = execution_session_->endSession()) {
        execution_session_->reportError(std::move(err));
    }
}

const llvm::DataLayout& OrcJitEngine::get_data_layout() const {
    return layout_;
}

llvm::orc::JITDylib& OrcJitEngine::get_main_jit_dylib() {
    return main_jit_dylib_;
}

llvm::Error OrcJitEngine::add_module(llvm::orc::ThreadSafeModule tsm, llvm::orc::ResourceTrackerSP rt) {
    if (!rt) {
        rt = main_jit_dylib_.getDefaultResourceTracker();
    }
    return compiler_layer_.add(rt, std::move(tsm));
}

llvm::Expected<llvm::JITEvaluatedSymbol> OrcJitEngine::lookup(llvm::StringRef name) {
    return execution_session_->lookup({&main_jit_dylib_}, mangle_(name.str()));
}