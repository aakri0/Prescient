// Per-pass timing instrumentation (issue #14).
//
// This translation unit is linked into the same IRComplexityEstimator.so
// plugin as IRComplexityPass.cpp. It is wired up from the plugin's
// llvmGetPassPluginInfo callback, which calls registerPassTiming() below.
//
// It records how long every *function-level* pass takes to process every
// function and writes one CSV row per (function, pass) pair. Module-level
// and loop-level passes are silently ignored, as required.
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/Any.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include <chrono>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <utility>

using namespace llvm;

static cl::opt<std::string> TimingOutput(
    "timing-output",
    cl::desc("Path to write the per-pass timing CSV"),
    cl::value_desc("path"), cl::init("timings.csv"));

namespace {

using Clock = std::chrono::steady_clock;

// Return the function name carried by an instrumentation IR unit, or an
// empty StringRef when the unit is not a Function. The new pass manager
// passes IR units wrapped in llvm::Any as `const Function *` (function
// passes), `const Module *`, `const Loop *` or `const LazyCallGraph::SCC *`.
// any_isa is deprecated in LLVM 17, so the pointer-returning any_cast
// overload is used instead.
StringRef functionName(const Any &IR) {
  if (const Function *const *FP = any_cast<const Function *>(&IR))
    return (*FP)->getName();
  return StringRef();
}

// Write S as a quoted CSV field. Pass names such as
// "RequireAnalysisPass<A, B, C>" contain commas, so every field is quoted
// and any embedded double quote is doubled per RFC 4180.
void writeCsvField(raw_ostream &O, StringRef S) {
  O << '"';
  for (char C : S) {
    if (C == '"')
      O << '"';
    O << C;
  }
  O << '"';
}

std::string isoTimestamp() {
  std::time_t T = std::time(nullptr);
  char Buf[32];
  std::tm TM;
#if defined(_WIN32)
  localtime_s(&TM, &T);
#else
  localtime_r(&T, &TM);
#endif
  std::strftime(Buf, sizeof(Buf), "%Y-%m-%dT%H:%M:%S", &TM);
  return std::string(Buf);
}

// Records pass start times and emits one CSV row per finished pass. A
// single instance lives for the whole process (see registerPassTiming).
class PassTimingInstrumentation {
public:
  void registerCallbacks(PassInstrumentationCallbacks &PIC) {
    PIC.registerBeforeNonSkippedPassCallback(
        [this](StringRef PassID, Any IR) { onBeforePass(PassID, IR); });
    PIC.registerAfterPassCallback(
        [this](StringRef PassID, Any IR, const PreservedAnalyses &) {
          onAfterPass(PassID, IR);
        });
  }

private:
  std::map<std::pair<std::string, std::string>, Clock::time_point> StartTimes;
  std::unique_ptr<raw_fd_ostream> OS;
  bool HeaderWritten = false;

  // Open the CSV lazily and write the header on first use.
  raw_fd_ostream *stream() {
    if (HeaderWritten)
      return OS && !OS->has_error() ? OS.get() : nullptr;
    HeaderWritten = true;
    std::string Path = TimingOutput;
    std::error_code EC;
    OS = std::make_unique<raw_fd_ostream>(Path, EC, sys::fs::OF_Text);
    if (EC) {
      errs() << "[PassTiming] error: cannot open " << Path << ": "
             << EC.message() << "\n";
      OS.reset();
      return nullptr;
    }
    *OS << "function_name,pass_name,time_us,timestamp\n";
    OS->flush();
    return OS.get();
  }

  void onBeforePass(StringRef PassID, const Any &IR) {
    StringRef FN = functionName(IR);
    if (FN.empty())
      return; // not a function-level pass — ignore
    StartTimes[{PassID.str(), FN.str()}] = Clock::now();
  }

  void onAfterPass(StringRef PassID, const Any &IR) {
    StringRef FN = functionName(IR);
    if (FN.empty())
      return;
    auto It = StartTimes.find({PassID.str(), FN.str()});
    if (It == StartTimes.end())
      return;
    auto Elapsed = Clock::now() - It->second;
    StartTimes.erase(It);

    long long US =
        std::chrono::duration_cast<std::chrono::microseconds>(Elapsed).count();
    if (US < 0)
      US = 0; // time_us must never be negative

    if (raw_fd_ostream *O = stream()) {
      writeCsvField(*O, FN);
      *O << ",";
      writeCsvField(*O, PassID);
      *O << "," << US << "," << isoTimestamp() << "\n";
      // Flush every row so the CSV is intact even if a later pass crashes.
      O->flush();
    }
  }
};

} // namespace

// Register the timing callbacks on an arbitrary PassInstrumentationCallbacks.
// The instrumentation object is process-lived so its callbacks stay valid
// for the entire opt run. Used both for opt's own PIC and for the private
// PIC that AdaptivePipeline.cpp drives its nested pipelines through, so
// adaptive-run timings land in the same CSV.
void registerTimingCallbacks(PassInstrumentationCallbacks &PIC) {
  static PassTimingInstrumentation Instrumentation;
  Instrumentation.registerCallbacks(PIC);
}

// Called from llvmGetPassPluginInfo (IRComplexityPass.cpp).
void registerPassTiming(PassBuilder &PB) {
  if (PassInstrumentationCallbacks *PIC = PB.getPassInstrumentationCallbacks())
    registerTimingCallbacks(*PIC);
}
