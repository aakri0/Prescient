// IR complexity feature extraction pass.
// Skeleton in #6; feature groups added in #7-#12; JSON export in #13.
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"

#include <iterator>
#include <string>

using namespace llvm;

namespace {

// Per-function results — the field set defines the JSON schema (issue #13).
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

  // #11 — type complexity
  unsigned type_complexity_score = 0;
  float type_complexity_normalized = 0.0f;
  unsigned max_pointer_depth = 0;
  unsigned struct_field_total = 0;

  // #12 — alias proxy density
  unsigned load_count = 0;
  unsigned store_count = 0;
  unsigned gep_count = 0;
  unsigned memory_intrinsic_count = 0;
  unsigned total_memory_ops = 0;
  float alias_proxy_density = 0.0f;
  float memory_write_ratio = 0.0f;
};

// Recursively score a type by structural depth. LLVM 17 uses opaque pointers
// by default, so a pointer's pointee is usually unknowable from the type
// alone — opaque pointers contribute depth 1 and cannot be descended into.
unsigned scoreType(Type *T, unsigned &maxPtrDepth, unsigned &structFields,
                   unsigned ptrDepth = 0, unsigned depth = 0) {
  if (depth > 10)
    return 10; // cycle guard for self-referential types

  if (T->isPointerTy()) {
    unsigned d = ptrDepth + 1;
    if (d > maxPtrDepth)
      maxPtrDepth = d;
    if (T->isOpaquePointerTy())
      return 1; // opaque pointer — pointee type is not recoverable
    return 1 + scoreType(T->getNonOpaquePointerElementType(), maxPtrDepth,
                         structFields, d, depth + 1);
  }
  if (auto *ST = dyn_cast<StructType>(T)) {
    unsigned s = ST->getNumElements();
    structFields += ST->getNumElements();
    for (Type *elem : ST->elements())
      s += scoreType(elem, maxPtrDepth, structFields, ptrDepth, depth + 1);
    return s;
  }
  if (auto *AT = dyn_cast<ArrayType>(T))
    return 1 + scoreType(AT->getElementType(), maxPtrDepth, structFields,
                         ptrDepth, depth + 1);
  if (auto *VT = dyn_cast<VectorType>(T))
    return 1 + scoreType(VT->getElementType(), maxPtrDepth, structFields,
                         ptrDepth, depth + 1);
  return 1; // primitive type
}

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

  // #10 PHI density / #11 type complexity / #12 memory ops — single scan
  unsigned ptrDepth = 0, structFields = 0;
  for (BasicBlock &BB : F) {
    unsigned bbPhi = 0;
    for (Instruction &I : BB) {
      if (auto *PN = dyn_cast<PHINode>(&I)) {
        FF.phi_node_count++;
        bbPhi++;
        FF.phi_node_incoming_edges += PN->getNumIncomingValues();
      }
      FF.type_complexity_score +=
          scoreType(I.getType(), ptrDepth, structFields);
      for (Use &U : I.operands())
        FF.type_complexity_score +=
            scoreType(U->getType(), ptrDepth, structFields);
      if (isa<LoadInst>(I))
        FF.load_count++;
      else if (isa<StoreInst>(I))
        FF.store_count++;
      else if (isa<GetElementPtrInst>(I))
        FF.gep_count++;
      if (auto *II = dyn_cast<IntrinsicInst>(&I)) {
        if (II->getIntrinsicID() == Intrinsic::memcpy ||
            II->getIntrinsicID() == Intrinsic::memmove ||
            II->getIntrinsicID() == Intrinsic::memset)
          FF.memory_intrinsic_count++;
      }
    }
    if (bbPhi > FF.max_phi_in_single_bb)
      FF.max_phi_in_single_bb = bbPhi;
  }
  FF.max_pointer_depth = ptrDepth;
  FF.struct_field_total = structFields;
  if (FF.instruction_count > 0) {
    FF.phi_density = (float)FF.phi_node_count / FF.instruction_count;
    FF.type_complexity_normalized =
        (float)FF.type_complexity_score / FF.instruction_count;
  }

  // #12 — alias proxy density
  FF.total_memory_ops = FF.load_count + FF.store_count + FF.gep_count +
                        FF.memory_intrinsic_count;
  if (FF.instruction_count > 0)
    FF.alias_proxy_density =
        (float)FF.total_memory_ops / FF.instruction_count;
  unsigned ls = FF.load_count + FF.store_count;
  if (ls > 0)
    FF.memory_write_ratio = (float)FF.store_count / ls;

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
           << " mem_ops=" << FF.total_memory_ops
           << " alias_density=" << FF.alias_proxy_density << "\n";

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
