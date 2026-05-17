// Stub — implementation added in issues #6-#13
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

llvm::PassPluginLibraryInfo getIRComplexityPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "IRComplexity", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {}};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getIRComplexityPluginInfo();
}
