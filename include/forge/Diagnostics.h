#pragma once

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"

#include <chrono>
#include <cstdint>

namespace llvm {
class Function;
}

namespace forge {

struct PassDiagnostics {
  uint64_t InstructionsBefore = 0;
  uint64_t InstructionsAfter = 0;
  uint64_t ExpressionsExamined = 0;
  uint64_t ExpressionsEliminated = 0;
  uint64_t CrossBlockReuses = 0;
  uint64_t LoopInvariantCandidates = 0;
  uint64_t InstructionsHoisted = 0;
  llvm::StringMap<uint64_t> Rejections;
};

bool diagnosticsEnabled();
uint64_t countInstructions(const llvm::Function &F);
void reject(PassDiagnostics &D, llvm::StringRef Reason);
void writeDiagnostics(llvm::StringRef PassName, const llvm::Function &F,
                      const PassDiagnostics &D, uint64_t ElapsedNanoseconds);

class ScopedPassTimer {
public:
  ScopedPassTimer();
  uint64_t elapsedNanoseconds() const;

private:
  std::chrono::steady_clock::time_point Start;
};

} // namespace forge
