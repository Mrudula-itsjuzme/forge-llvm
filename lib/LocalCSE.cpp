#include "forge/Expression.h"
#include "forge/Passes.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CFG.h"

#define DEBUG_TYPE "forge-local-cse"

using namespace llvm;

STATISTIC(NumLocalEliminated,
          "Number of redundant expressions eliminated by Forge LocalCSE");

namespace forge {

PreservedAnalyses LocalCSEPass::run(Function &F,
                                    FunctionAnalysisManager &) {
  bool Changed = false;
  for (BasicBlock &BB : F) {
    DenseMap<ExpressionKey, SmallVector<BinaryOperator *, 2>,
             ExpressionKeyInfo>
        Available;

    for (auto It = BB.begin(), End = BB.end(); It != End;) {
      auto *BO = dyn_cast<BinaryOperator>(&*It++);
      if (!BO || !isEligibleIntegerExpression(*BO))
        continue;

      auto &Candidates = Available[makeExpressionKey(*BO)];
      BinaryOperator *Equivalent = nullptr;
      for (BinaryOperator *Candidate : Candidates)
        if (areEquivalentExpressions(*BO, *Candidate)) {
          Equivalent = Candidate;
          break;
        }

      if (!Equivalent) {
        Candidates.push_back(BO);
        continue;
      }

      BO->replaceAllUsesWith(Equivalent);
      BO->eraseFromParent();
      ++NumLocalEliminated;
      Changed = true;
    }
  }

  if (!Changed)
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

} // namespace forge

