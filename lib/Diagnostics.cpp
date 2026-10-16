#include "forge/Diagnostics.h"

#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include <mutex>
#include <system_error>

using namespace llvm;

namespace {

cl::opt<std::string> DiagnosticsPath(
    "forge-diagnostics-jsonl", cl::init(""),
    cl::desc("Append opt-in Forge pass diagnostics as JSON Lines"));

std::mutex DiagnosticsMutex;

} // namespace

namespace forge {

bool diagnosticsEnabled() { return !DiagnosticsPath.empty(); }

uint64_t countInstructions(const Function &F) {
  uint64_t Count = 0;
  for (const BasicBlock &BB : F)
    Count += BB.size();
  return Count;
}

void reject(PassDiagnostics &D, StringRef Reason) { ++D.Rejections[Reason]; }

void writeDiagnostics(StringRef PassName, const Function &F,
                      const PassDiagnostics &D, uint64_t ElapsedNanoseconds) {
  if (!diagnosticsEnabled())
    return;

  json::Object Rejections;
  for (const auto &Entry : D.Rejections)
    Rejections[Entry.getKey()] = static_cast<int64_t>(Entry.getValue());

  json::Object Record{
      {"schema_version", 1},
      {"record_type", "pass_diagnostics"},
      {"pass", PassName},
      {"function", F.getName()},
      {"instructions_before", static_cast<int64_t>(D.InstructionsBefore)},
      {"instructions_after", static_cast<int64_t>(D.InstructionsAfter)},
      {"instruction_delta",
       static_cast<int64_t>(D.InstructionsAfter) -
           static_cast<int64_t>(D.InstructionsBefore)},
      {"expressions_examined", static_cast<int64_t>(D.ExpressionsExamined)},
      {"expressions_eliminated",
       static_cast<int64_t>(D.ExpressionsEliminated)},
      {"cross_block_reuses", static_cast<int64_t>(D.CrossBlockReuses)},
      {"loop_invariant_candidates",
       static_cast<int64_t>(D.LoopInvariantCandidates)},
      {"instructions_hoisted", static_cast<int64_t>(D.InstructionsHoisted)},
      {"elapsed_nanoseconds", static_cast<int64_t>(ElapsedNanoseconds)},
      {"rejections", std::move(Rejections)},
  };

  std::lock_guard<std::mutex> Guard(DiagnosticsMutex);
  std::error_code EC;
  raw_fd_ostream Output(DiagnosticsPath, EC, sys::fs::OF_Append);
  if (EC) {
    errs() << "forge: cannot write diagnostics to " << DiagnosticsPath << ": "
           << EC.message() << "\n";
    return;
  }
  Output << formatv("{0}\n", json::Value(std::move(Record)));
}

ScopedPassTimer::ScopedPassTimer() : Start(std::chrono::steady_clock::now()) {}

uint64_t ScopedPassTimer::elapsedNanoseconds() const {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - Start)
          .count());
}

} // namespace forge
