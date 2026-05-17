// IR complexity feature extraction pass — skeleton (issue #6).
// Feature groups are added in issues #7-#12; JSON export in #13.
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

using namespace llvm;

namespace {

// Per-function results. Fields are added in issues #7-#12.
struct FunctionFeatures {
  std::string function_name;
};

struct IRComplexityPass : PassInfoMixin<IRComplexityPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    if (F.isDeclaration())
      return PreservedAnalyses::all();
    errs() << "[IRComplexity] Analyzing function: " << F.getName() << "\n";
    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

llvm::PassPluginLibraryInfo getIRComplexityPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "IRComplexity", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "ir-complexity") {
                    FPM.addPass(IRComplexityPass());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getIRComplexityPluginInfo();
}
