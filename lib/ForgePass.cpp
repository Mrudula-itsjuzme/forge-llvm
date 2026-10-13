#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <functional>
#include <utility>

using namespace llvm;

namespace {

struct ExpressionKey {
  unsigned Opcode;
  Type *ResultType;
  Value *LHS;
  Value *RHS;

  bool operator==(const ExpressionKey &Other) const {
    return Opcode == Other.Opcode && ResultType == Other.ResultType &&
           LHS == Other.LHS && RHS == Other.RHS;
  }
};

struct ExpressionKeyInfo {
  static inline ExpressionKey getEmptyKey() {
    return {~0U, nullptr, nullptr, nullptr};
  }
  static inline ExpressionKey getTombstoneKey() {
    return {~0U - 1, nullptr, nullptr, nullptr};
  }
  static unsigned getHashValue(const ExpressionKey &Key) {
    return static_cast<unsigned>(
        hash_combine(Key.Opcode, Key.ResultType, Key.LHS, Key.RHS));
  }
  static bool isEqual(const ExpressionKey &A, const ExpressionKey &B) {
    return A == B;
  }
};

static bool isEligible(const BinaryOperator &BO) {
  if (!BO.getType()->isIntOrIntVectorTy())
    return false;

  // LLVM versions newer than the CI baseline may attach additional semantics
  // to `or` (for example, `disjoint`). Keep it out until those flags are
  // compared explicitly across every supported LLVM version.
  if (BO.getOpcode() == Instruction::Or)
    return false;

  // Literal undef is use-sensitive, so two syntactically equal expressions
  // need not select the same value. Reject it, and explicit poison, outright.
  for (const Value *Operand : BO.operands()) {
    const auto *C = dyn_cast<Constant>(Operand);
    if (isa<UndefValue>(Operand) || isa<PoisonValue>(Operand) ||
        (C && C->containsUndefOrPoisonElement()))
      return false;
  }

  return true;
}

static bool haveSameSemanticFlags(const BinaryOperator &A,
                                  const BinaryOperator &B) {
  if (const auto *AO = dyn_cast<OverflowingBinaryOperator>(&A)) {
    const auto *BO = cast<OverflowingBinaryOperator>(&B);
    if (AO->hasNoUnsignedWrap() != BO->hasNoUnsignedWrap() ||
        AO->hasNoSignedWrap() != BO->hasNoSignedWrap())
      return false;
  }
  if (const auto *AE = dyn_cast<PossiblyExactOperator>(&A)) {
    const auto *BE = cast<PossiblyExactOperator>(&B);
    if (AE->isExact() != BE->isExact())
      return false;
  }
  return true;
}

static ExpressionKey makeKey(BinaryOperator &BO) {
  Value *LHS = BO.getOperand(0);
  Value *RHS = BO.getOperand(1);
  if (BO.isCommutative() && std::less<Value *>{}(RHS, LHS))
    std::swap(LHS, RHS);
  return {BO.getOpcode(), BO.getType(), LHS, RHS};
}

class ForgePass : public PassInfoMixin<ForgePass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool Changed = false;

    for (BasicBlock &BB : F) {
      DenseMap<ExpressionKey, SmallVector<BinaryOperator *, 2>,
               ExpressionKeyInfo>
          Available;

      for (auto It = BB.begin(), End = BB.end(); It != End;) {
        Instruction &I = *It++;
        auto *BO = dyn_cast<BinaryOperator>(&I);
        if (!BO || !isEligible(*BO))
          continue;

        ExpressionKey Key = makeKey(*BO);
        auto &Candidates = Available[Key];
        BinaryOperator *Equivalent = nullptr;
        for (BinaryOperator *Candidate : Candidates) {
          // This checks result type plus semantic subclass data, including
          // nsw/nuw/exact flags. Operands are checked separately to permit
          // reversed order only for commutative operations.
          if (BO->isSameOperationAs(Candidate) &&
              haveSameSemanticFlags(*BO, *Candidate)) {
            Equivalent = Candidate;
            break;
          }
        }

        if (!Equivalent) {
          Candidates.push_back(BO);
          continue;
        }

        BO->replaceAllUsesWith(Equivalent);
        BO->eraseFromParent();
        Changed = true;
      }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ForgePass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name != "forge-redundant-expr-elim")
                    return false;
                  FPM.addPass(ForgePass());
                  return true;
                });
          }};
}
