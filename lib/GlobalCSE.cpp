#include "forge/Expression.h"
#include "forge/Passes.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Dominators.h"

#define DEBUG_TYPE "forge-global-cse"

using namespace llvm;

STATISTIC(NumGlobalEliminated,
          "Number of redundant expressions eliminated by Forge GlobalCSE");

namespace forge {
namespace {

using AvailableMap =
    DenseMap<ExpressionKey, SmallVector<BinaryOperator *, 2>, ExpressionKeyInfo>;

static bool processNode(DomTreeNode *Node, AvailableMap &Available) {
  bool Changed = false;
  SmallVector<ExpressionKey, 8> Pushed;

  for (auto It = Node->getBlock()->begin(), End = Node->getBlock()->end();
       It != End;) {
    auto *BO = dyn_cast<BinaryOperator>(&*It++);
    if (!BO || !isEligibleIntegerExpression(*BO))
      continue;

    ExpressionKey Key = makeExpressionKey(*BO);
    auto &Candidates = Available[Key];
    BinaryOperator *Equivalent = nullptr;
    for (BinaryOperator *Candidate : Candidates)
      if (areEquivalentExpressions(*BO, *Candidate)) {
        Equivalent = Candidate;
        break;
      }

    if (Equivalent) {
      BO->replaceAllUsesWith(Equivalent);
      BO->eraseFromParent();
      ++NumGlobalEliminated;
      Changed = true;
      continue;
    }

    Candidates.push_back(BO);
    Pushed.push_back(Key);
  }

  for (DomTreeNode *Child : *Node)
    Changed |= processNode(Child, Available);

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
  DomTreeNode *Root = DT.getRootNode();
  if (!Root)
    return PreservedAnalyses::all();

  AvailableMap Available;
  if (!processNode(Root, Available))
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  PA.preserve<DominatorTreeAnalysis>();
  return PA;
}

} // namespace forge
