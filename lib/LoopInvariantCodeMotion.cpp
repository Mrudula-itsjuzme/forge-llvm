#include "forge/Diagnostics.h"
#include "forge/Expression.h"
#include "forge/Passes.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"

#include <optional>

#define DEBUG_TYPE "forge-licm"

using namespace llvm;

STATISTIC(NumLoopHoisted,
          "Number of loop-invariant expressions hoisted by Forge LICM");

namespace forge {
namespace {

static bool processInnermostLoop(Loop &L, DominatorTree &DT,
                                 PassDiagnostics *D,
                                 OptimizationRemarkEmitter &ORE) {
  BasicBlock *Preheader = L.getLoopPreheader();
  if (!Preheader) {
    if (D)
      reject(*D, "missing_loop_preheader");
    return false;
  }

  bool Changed = false;
  bool Progress = true;
  while (Progress) {
    Progress = false;
    for (BasicBlock *BB : L.blocks()) {
      for (auto It = BB->begin(), End = BB->end(); It != End;) {
        auto *BO = dyn_cast<BinaryOperator>(&*It++);
        if (!BO)
          continue;
        if (D)
          ++D->ExpressionsExamined;
        ExpressionRejectionReason Reason = classifyIntegerExpression(*BO);
        if (Reason != ExpressionRejectionReason::Eligible) {
          if (D)
            reject(*D, rejectionReasonName(Reason));
          continue;
        }
        if (!L.hasLoopInvariantOperands(BO)) {
          if (D)
            reject(*D, "loop_variant_operand");
          continue;
        }
        if (D)
          ++D->LoopInvariantCandidates;
        if (!isSafeToSpeculativelyExecute(BO, nullptr, nullptr, &DT)) {
          if (D)
            reject(*D, "unsafe_to_speculate");
          continue;
        }

        ORE.emit([&]() {
          return OptimizationRemark(DEBUG_TYPE, "Hoisted", BO)
                 << "hoisted loop-invariant integer expression";
        });
        BO->moveBefore(Preheader->getTerminator());
        ++NumLoopHoisted;
        if (D)
          ++D->InstructionsHoisted;
        Changed = Progress = true;
      }
    }
  }
  return Changed;
}

static bool processLoopTree(Loop &L, DominatorTree &DT, PassDiagnostics *D,
                            OptimizationRemarkEmitter &ORE) {
  bool Changed = false;
  for (Loop *SubLoop : L.getSubLoops())
    Changed |= processLoopTree(*SubLoop, DT, D, ORE);
  if (L.isInnermost())
    Changed |= processInnermostLoop(L, DT, D, ORE);
  else if (D)
    reject(*D, "non_innermost_loop");
  return Changed;
}

} // namespace

PreservedAnalyses LoopInvariantCodeMotionPass::run(
    Function &F, FunctionAnalysisManager &AM) {
  LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
  OptimizationRemarkEmitter &ORE =
      AM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  const bool Diagnose = diagnosticsEnabled();
  std::optional<ScopedPassTimer> Timer;
  if (Diagnose)
    Timer.emplace();
  PassDiagnostics D;
  if (Diagnose)
    D.InstructionsBefore = countInstructions(F);
  bool Changed = false;
  for (Loop *L : LI)
    Changed |= processLoopTree(*L, DT, Diagnose ? &D : nullptr, ORE);

  if (Diagnose) {
    D.InstructionsAfter = countInstructions(F);
    writeDiagnostics(DEBUG_TYPE, F, D, Timer->elapsedNanoseconds());
  }

  if (!Changed)
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  PA.preserve<DominatorTreeAnalysis>();
  PA.preserve<LoopAnalysis>();
  return PA;
}

} // namespace forge
