#include "forge/Expression.h"

#include "llvm/ADT/Hashing.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Operator.h"

#include <functional>
#include <utility>

using namespace llvm;

namespace forge {

bool ExpressionKey::operator==(const ExpressionKey &Other) const {
  return Opcode == Other.Opcode && ResultType == Other.ResultType &&
         LHS == Other.LHS && RHS == Other.RHS;
}

ExpressionKey ExpressionKeyInfo::getEmptyKey() {
  return {~0U, nullptr, nullptr, nullptr};
}

ExpressionKey ExpressionKeyInfo::getTombstoneKey() {
  return {~0U - 1, nullptr, nullptr, nullptr};
}

unsigned ExpressionKeyInfo::getHashValue(const ExpressionKey &Key) {
  return static_cast<unsigned>(
      hash_combine(Key.Opcode, Key.ResultType, Key.LHS, Key.RHS));
}

bool ExpressionKeyInfo::isEqual(const ExpressionKey &A,
                                const ExpressionKey &B) {
  return A == B;
}

bool isEligibleIntegerExpression(const BinaryOperator &BO) {
  if (!BO.getType()->isIntOrIntVectorTy())
    return false;

  // Newer LLVM versions can attach `disjoint` semantics to Or. Forge's LLVM
  // 18 baseline has no portable query for that flag, so exclude Or entirely.
  if (BO.getOpcode() == Instruction::Or)
    return false;

  for (const Value *Operand : BO.operands()) {
    const auto *C = dyn_cast<Constant>(Operand);
    if (isa<UndefValue>(Operand) || isa<PoisonValue>(Operand) ||
        (C && C->containsUndefOrPoisonElement()))
      return false;
  }
  return true;
}

ExpressionKey makeExpressionKey(const BinaryOperator &BO) {
  Value *LHS = BO.getOperand(0);
  Value *RHS = BO.getOperand(1);
  if (BO.isCommutative() && std::less<Value *>{}(RHS, LHS))
    std::swap(LHS, RHS);
  return {BO.getOpcode(), BO.getType(), LHS, RHS};
}

bool haveSameSemanticFlags(const BinaryOperator &A, const BinaryOperator &B) {
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

bool areEquivalentExpressions(const BinaryOperator &A,
                              const BinaryOperator &B) {
  if (!A.isSameOperationAs(&B) || !haveSameSemanticFlags(A, B))
    return false;
  return makeExpressionKey(A) == makeExpressionKey(B);
}

} // namespace forge
