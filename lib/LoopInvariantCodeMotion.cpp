#include "forge/Expression.h"
#include "forge/Passes.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"

#define DEBUG_TYPE "forge-licm"

using namespace llvm;

STATISTIC(NumLoopHoisted,
          "Number of loop-invariant expressions hoisted by Forge LICM");

namespace forge {
namespace {

static bool processInnermostLoop(Loop &L, DominatorTree &DT) {
  if (!L.isInnermost())
    return false;
  BasicBlock *Preheader = L.getLoopPreheader();
  if (!Preheader)
    return false;

  bool Changed = false;
  bool Progress = true;
  while (Progress) {
    Progress = false;
    for (BasicBlock *BB : L.blocks()) {
      for (auto It = BB->begin(), End = BB->end(); It != End;) {
        auto *BO = dyn_cast<BinaryOperator>(&*It++);
        if (!BO || !isEligibleIntegerExpression(*BO) ||
            !L.hasLoopInvariantOperands(BO) ||
            !isSafeToSpeculativelyExecute(BO, nullptr, nullptr, &DT))
          continue;

        BO->moveBefore(Preheader->getTerminator());
        ++NumLoopHoisted;
        Changed = Progress = true;
      }
    }
  }
  return Changed;
}

static bool processLoopTree(Loop &L, DominatorTree &DT) {
  bool Changed = false;
  for (Loop *SubLoop : L.getSubLoops())
    Changed |= processLoopTree(*SubLoop, DT);
  Changed |= processInnermostLoop(L, DT);
  return Changed;
}

} // namespace

PreservedAnalyses LoopInvariantCodeMotionPass::run(
    Function &F, FunctionAnalysisManager &AM) {
  LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
  bool Changed = false;
  for (Loop *L : LI)
    Changed |= processLoopTree(*L, DT);

  if (!Changed)
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  PA.preserve<DominatorTreeAnalysis>();
  PA.preserve<LoopAnalysis>();
  return PA;
}

} // namespace forge
