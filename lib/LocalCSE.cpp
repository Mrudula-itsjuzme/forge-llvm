#include "forge/Diagnostics.h"
#include "forge/Expression.h"
#include "forge/Passes.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/IR/CFG.h"

#include <optional>

#define DEBUG_TYPE "forge-local-cse"

using namespace llvm;

STATISTIC(NumLocalEliminated,
          "Number of redundant expressions eliminated by Forge LocalCSE");

namespace forge {

PreservedAnalyses LocalCSEPass::run(Function &F,
                                    FunctionAnalysisManager &AM) {
  const bool Diagnose = diagnosticsEnabled();
  std::optional<ScopedPassTimer> Timer;
  if (Diagnose)
    Timer.emplace();
  PassDiagnostics D;
  if (Diagnose)
    D.InstructionsBefore = countInstructions(F);
  OptimizationRemarkEmitter &ORE =
      AM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  bool Changed = false;
  for (BasicBlock &BB : F) {
    DenseMap<ExpressionKey, SmallVector<BinaryOperator *, 2>,
             ExpressionKeyInfo>
        Available;

    for (auto It = BB.begin(), End = BB.end(); It != End;) {
      auto *BO = dyn_cast<BinaryOperator>(&*It++);
      if (!BO)
        continue;
      if (Diagnose)
        ++D.ExpressionsExamined;
      ExpressionRejectionReason Reason = classifyIntegerExpression(*BO);
      if (Reason != ExpressionRejectionReason::Eligible) {
        if (Diagnose)
          reject(D, rejectionReasonName(Reason));
        continue;
      }

      auto &Candidates = Available[makeExpressionKey(*BO)];
      BinaryOperator *Equivalent = nullptr;
      for (BinaryOperator *Candidate : Candidates)
        if (areEquivalentExpressions(*BO, *Candidate)) {
          Equivalent = Candidate;
          break;
        }

      if (!Equivalent) {
        if (Diagnose)
          reject(D, Candidates.empty() ? "no_available_equivalent"
                                       : "semantic_flags_mismatch");
        Candidates.push_back(BO);
        continue;
      }

      ORE.emit([&]() {
        return OptimizationRemark(DEBUG_TYPE, "Eliminated", BO)
               << "eliminated redundant integer expression";
      });
      BO->replaceAllUsesWith(Equivalent);
      BO->eraseFromParent();
      ++NumLocalEliminated;
      if (Diagnose)
        ++D.ExpressionsEliminated;
      Changed = true;
    }
  }

  if (Diagnose) {
    D.InstructionsAfter = countInstructions(F);
    writeDiagnostics(DEBUG_TYPE, F, D, Timer->elapsedNanoseconds());
  }

  if (!Changed)
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

} // namespace forge
