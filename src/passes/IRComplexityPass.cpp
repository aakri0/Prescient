// IR complexity feature extraction pass.
// Skeleton in #6; feature groups added in #7-#12; JSON export in #13.
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"

#include <iterator>
#include <string>

using namespace llvm;

namespace {

// Per-function results. More fields are added in issues #11-#12.
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

  // #9 — loop metrics
  unsigned loop_count = 0;
  unsigned max_loop_depth = 0;
  unsigned loop_body_instruction_count = 0;
  float loop_instruction_ratio = 0.0f;

  // #10 — PHI node density
  unsigned phi_node_count = 0;
  float phi_density = 0.0f;
  unsigned max_phi_in_single_bb = 0;
  unsigned phi_node_incoming_edges = 0;
};

FunctionFeatures analyzeFunction(Function &F, FunctionAnalysisManager &FAM) {
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

  // #9 — loop metrics
  LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
  auto Loops = LI.getLoopsInPreorder();
  FF.loop_count = Loops.size();
  for (Loop *L : Loops)
    if (L->getLoopDepth() > FF.max_loop_depth)
      FF.max_loop_depth = L->getLoopDepth();
  // Iterate top-level loops only: their block lists already cover nested
  // loops, so this avoids counting nested-loop instructions twice.
  for (Loop *L : LI)
    for (BasicBlock *BB : L->getBlocks())
      FF.loop_body_instruction_count += BB->size();
  if (FF.instruction_count > 0)
    FF.loop_instruction_ratio =
        (float)FF.loop_body_instruction_count / FF.instruction_count;

  // #10 — PHI node density
  for (BasicBlock &BB : F) {
    unsigned bbPhi = 0;
    for (Instruction &I : BB) {
      if (auto *PN = dyn_cast<PHINode>(&I)) {
        FF.phi_node_count++;
        bbPhi++;
        FF.phi_node_incoming_edges += PN->getNumIncomingValues();
      }
    }
    if (bbPhi > FF.max_phi_in_single_bb)
      FF.max_phi_in_single_bb = bbPhi;
  }
  if (FF.instruction_count > 0)
    FF.phi_density = (float)FF.phi_node_count / FF.instruction_count;

  return FF;
}

struct IRComplexityPass : PassInfoMixin<IRComplexityPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    if (F.isDeclaration())
      return PreservedAnalyses::all();
    errs() << "[IRComplexity] Analyzing function: " << F.getName() << "\n";

    FunctionFeatures FF = analyzeFunction(F, FAM);
    errs() << "[IRComplexity] " << FF.function_name
           << ": insts=" << FF.instruction_count
           << " cyclomatic=" << FF.cyclomatic_complexity
           << " loops=" << FF.loop_count
           << " phis=" << FF.phi_node_count << "\n";

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
