// IR complexity feature extraction pass.
// Skeleton in #6; feature groups added in #7-#12; JSON export in #13.
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"

#include <iterator>
#include <string>

using namespace llvm;

namespace {

// Per-function results. More fields are added in issues #9-#12.
struct FunctionFeatures {
  std::string function_name;

  // #7 — function size
  unsigned instruction_count = 0;
  unsigned basic_block_count = 0;
  unsigned argument_count = 0;
  unsigned call_site_count = 0;

  // #8 — cyclomatic complexity
  unsigned cfg_edge_count = 0;
  unsigned cyclomatic_complexity = 0;
  unsigned max_bb_successors = 0;
};

FunctionFeatures analyzeFunction(Function &F) {
  FunctionFeatures FF;
  FF.function_name = F.getName().str();

  // #7 — function size
  FF.basic_block_count = F.size();
  FF.argument_count = F.arg_size();
  for (BasicBlock &BB : F) {
    FF.instruction_count += BB.size();
    for (Instruction &I : BB)
      if (isa<CallInst>(I) || isa<InvokeInst>(I))
        FF.call_site_count++;
  }

  // #8 — cyclomatic complexity: M = E - N + 2
  for (BasicBlock &BB : F) {
    unsigned succ = std::distance(succ_begin(&BB), succ_end(&BB));
    FF.cfg_edge_count += succ;
    if (succ > FF.max_bb_successors)
      FF.max_bb_successors = succ;
  }
  // Guard against unsigned underflow for tiny CFGs.
  if (FF.cfg_edge_count + 2 > FF.basic_block_count)
    FF.cyclomatic_complexity = FF.cfg_edge_count - FF.basic_block_count + 2;
  else
    FF.cyclomatic_complexity = 1;

  return FF;
}

struct IRComplexityPass : PassInfoMixin<IRComplexityPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    if (F.isDeclaration())
      return PreservedAnalyses::all();
    errs() << "[IRComplexity] Analyzing function: " << F.getName() << "\n";

    FunctionFeatures FF = analyzeFunction(F);
    errs() << "[IRComplexity] " << FF.function_name
           << ": insts=" << FF.instruction_count
           << " bbs=" << FF.basic_block_count
           << " args=" << FF.argument_count
           << " calls=" << FF.call_site_count
           << " cyclomatic=" << FF.cyclomatic_complexity << "\n";

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
