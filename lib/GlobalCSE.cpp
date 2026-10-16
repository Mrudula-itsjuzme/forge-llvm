#include "forge/Diagnostics.h"
#include "forge/Expression.h"
#include "forge/Passes.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/IR/Dominators.h"

#include <optional>

#define DEBUG_TYPE "forge-global-cse"

using namespace llvm;

STATISTIC(NumGlobalEliminated,
          "Number of redundant expressions eliminated by Forge GlobalCSE");

namespace forge {
namespace {

using AvailableMap =
    DenseMap<ExpressionKey, SmallVector<BinaryOperator *, 2>, ExpressionKeyInfo>;

static bool processNode(DomTreeNode *Node, AvailableMap &Available,
                        PassDiagnostics *D, OptimizationRemarkEmitter &ORE) {
  bool Changed = false;
  SmallVector<ExpressionKey, 8> Pushed;

  for (auto It = Node->getBlock()->begin(), End = Node->getBlock()->end();
       It != End;) {
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

    ExpressionKey Key = makeExpressionKey(*BO);
    auto &Candidates = Available[Key];
    BinaryOperator *Equivalent = nullptr;
    for (BinaryOperator *Candidate : Candidates)
      if (areEquivalentExpressions(*BO, *Candidate)) {
        Equivalent = Candidate;
        break;
      }

    if (Equivalent) {
      const bool CrossBlock = BO->getParent() != Equivalent->getParent();
      ORE.emit([&]() {
        return OptimizationRemark(DEBUG_TYPE, "Eliminated", BO)
               << "reused dominating integer expression";
      });
      BO->replaceAllUsesWith(Equivalent);
      BO->eraseFromParent();
      ++NumGlobalEliminated;
      if (D) {
        ++D->ExpressionsEliminated;
        if (CrossBlock)
          ++D->CrossBlockReuses;
      }
      Changed = true;
      continue;
    }

    if (D)
      reject(*D, Candidates.empty() ? "no_dominating_equivalent"
                                    : "semantic_flags_mismatch");
    Candidates.push_back(BO);
    Pushed.push_back(Key);
  }

  for (DomTreeNode *Child : *Node)
    Changed |= processNode(Child, Available, D, ORE);

  for (const ExpressionKey &Key : reverse(Pushed)) {
    auto Found = Available.find(Key);
    assert(Found != Available.end() && !Found->second.empty());
    Found->second.pop_back();
    if (Found->second.empty())
      Available.erase(Found);
  }
  return Changed;
}

} // namespace

PreservedAnalyses GlobalCSEPass::run(Function &F,
                                     FunctionAnalysisManager &AM) {
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
  DomTreeNode *Root = DT.getRootNode();
  if (!Root) {
    if (Diagnose) {
      D.InstructionsAfter = D.InstructionsBefore;
      reject(D, "no_dominator_tree_root");
      writeDiagnostics(DEBUG_TYPE, F, D, Timer->elapsedNanoseconds());
    }
    return PreservedAnalyses::all();
  }

  AvailableMap Available;
  const bool Changed = processNode(Root, Available, Diagnose ? &D : nullptr, ORE);
  if (Diagnose) {
    D.InstructionsAfter = countInstructions(F);
    writeDiagnostics(DEBUG_TYPE, F, D, Timer->elapsedNanoseconds());
  }
  if (!Changed)
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  PA.preserve<DominatorTreeAnalysis>();
  return PA;
}

} // namespace forge
