// Adaptive optimisation pipeline pass (issue #21).
//
// Reads per-function compile-time predictions and customises the
// optimization pipeline per function: cheap functions get a lighter O1
// pipeline (and skip the vectorisers), while everything else gets the full
// O2 simplification pipeline so correctness is preserved.
//
// Linked into the same IRComplexityEstimator.so plugin and registered as
// the "adaptive-pipeline" pass from llvmGetPassPluginInfo.
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/ADT/Any.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <unordered_map>

using namespace llvm;

// Defined in src/timing/PassTimingWrapper.cpp (issue #14). Lets the nested
// pipelines run here be timed into the same CSV as a baseline O2 run.
void registerTimingCallbacks(PassInstrumentationCallbacks &PIC);

namespace {

enum class Tier { Low, Medium, High };

Tier parseTier(StringRef S) {
  if (S == "low")
    return Tier::Low;
  if (S == "high")
    return Tier::High;
  return Tier::Medium; // unknown / "medium" -> safe default
}

// Module pass that applies a per-function pipeline chosen from predictions.
class AdaptivePipelinePass : public PassInfoMixin<AdaptivePipelinePass> {
  std::unordered_map<std::string, Tier> Tiers;

  // Load predictions.json (path from COMPLEXITY_PREDICTIONS). Any failure
  // leaves the map empty, so every function falls back to the medium (full
  // O2) tier — the safe, correctness-preserving default.
  void loadPredictions() {
    const char *Path = std::getenv("COMPLEXITY_PREDICTIONS");
    if (!Path) {
      errs() << "[Adaptive] COMPLEXITY_PREDICTIONS not set — every function "
                "defaults to full O2\n";
      return;
    }
    ErrorOr<std::unique_ptr<MemoryBuffer>> Buf = MemoryBuffer::getFile(Path);
    if (!Buf) {
      errs() << "[Adaptive] cannot read predictions file '" << Path
             << "' — every function defaults to full O2\n";
      return;
    }
    Expected<json::Value> Parsed = json::parse((*Buf)->getBuffer());
    if (!Parsed) {
      consumeError(Parsed.takeError());
      errs() << "[Adaptive] malformed predictions JSON — every function "
                "defaults to full O2\n";
      return;
    }
    if (json::Array *Arr = Parsed->getAsArray()) {
      for (json::Value &Elem : *Arr) {
        json::Object *Obj = Elem.getAsObject();
        if (!Obj)
          continue;
        std::optional<StringRef> Name = Obj->getString("function_name");
        std::optional<StringRef> TierStr = Obj->getString("complexity_tier");
        if (Name && TierStr)
          Tiers[Name->str()] = parseTier(*TierStr);
      }
    }
    errs() << "[Adaptive] loaded " << Tiers.size() << " prediction(s) from "
           << Path << "\n";
  }

  // Functions absent from predictions.json default to the medium tier.
  Tier tierFor(StringRef Name) const {
    auto It = Tiers.find(Name.str());
    return It == Tiers.end() ? Tier::Medium : It->second;
  }

public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    loadPredictions();

    // A private instrumentation channel for the nested pipelines.
    PassInstrumentationCallbacks PIC;
    // Low-tier functions skip the vectorisers wherever they appear.
    PIC.registerShouldRunOptionalPassCallback(
        [this](StringRef PassID, Any IR) -> bool {
          if (PassID != "LoopVectorizePass" && PassID != "SLPVectorizerPass")
            return true;
          if (const Function *const *F = any_cast<const Function *>(&IR))
            if (tierFor((*F)->getName()) == Tier::Low)
              return false; // skip this pass
          return true;
        });
    // Record nested-pass timings into the active -timing-output CSV.
    registerTimingCallbacks(PIC);

    PassBuilder PB(/*TM=*/nullptr, PipelineTuningOptions(), std::nullopt, &PIC);
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    // Build one pipeline per tier up front, then reuse across functions.
    FunctionPassManager LowFPM = PB.buildFunctionSimplificationPipeline(
        OptimizationLevel::O1, ThinOrFullLTOPhase::None);
    FunctionPassManager MediumFPM = PB.buildFunctionSimplificationPipeline(
        OptimizationLevel::O2, ThinOrFullLTOPhase::None);
    FunctionPassManager HighFPM = PB.buildFunctionSimplificationPipeline(
        OptimizationLevel::O2, ThinOrFullLTOPhase::None);

    for (Function &F : M) {
      if (F.isDeclaration())
        continue;
      switch (tierFor(F.getName())) {
      case Tier::Low:
        errs() << "[Adaptive] " << F.getName()
               << ": tier=low, pipeline=O1, "
                  "skipped=LoopVectorizePass,SLPVectorizerPass\n";
        LowFPM.run(F, FAM);
        break;
      case Tier::Medium:
        errs() << "[Adaptive] " << F.getName()
               << ": tier=medium, pipeline=O2, skipped=none\n";
        MediumFPM.run(F, FAM);
        break;
      case Tier::High:
        errs() << "[Adaptive] " << F.getName()
               << ": tier=high, pipeline=O2, skipped=none\n";
        HighFPM.run(F, FAM);
        break;
      }
    }
    // The module has been optimised in place.
    return PreservedAnalyses::none();
  }

  static bool isRequired() { return true; }
};

} // namespace

// Called from llvmGetPassPluginInfo (IRComplexityPass.cpp) so the pass is
// usable as opt -passes="adaptive-pipeline".
void registerAdaptivePipeline(PassBuilder &PB) {
  PB.registerPipelineParsingCallback(
      [](StringRef Name, ModulePassManager &MPM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (Name == "adaptive-pipeline") {
          MPM.addPass(AdaptivePipelinePass());
          return true;
        }
        return false;
      });
}
