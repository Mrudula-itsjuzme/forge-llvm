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

ExpressionRejectionReason classifyIntegerExpression(const BinaryOperator &BO) {
  if (!BO.getType()->isIntOrIntVectorTy())
    return ExpressionRejectionReason::NonIntegerType;

  // Newer LLVM versions can attach `disjoint` semantics to Or. Forge's LLVM
  // 18 baseline has no portable query for that flag, so exclude Or entirely.
  if (BO.getOpcode() == Instruction::Or)
    return ExpressionRejectionReason::UnsupportedOrSemantics;

  for (const Value *Operand : BO.operands()) {
    const auto *C = dyn_cast<Constant>(Operand);
    if (isa<UndefValue>(Operand) || isa<PoisonValue>(Operand) ||
        (C && C->containsUndefOrPoisonElement()))
      return ExpressionRejectionReason::UndefOrPoisonOperand;
  }
  return ExpressionRejectionReason::Eligible;
}

bool isEligibleIntegerExpression(const BinaryOperator &BO) {
  return classifyIntegerExpression(BO) == ExpressionRejectionReason::Eligible;
}

StringRef rejectionReasonName(ExpressionRejectionReason Reason) {
  switch (Reason) {
  case ExpressionRejectionReason::Eligible:
    return "eligible";
  case ExpressionRejectionReason::NonIntegerType:
    return "non_integer_type";
  case ExpressionRejectionReason::UnsupportedOrSemantics:
    return "unsupported_or_semantics";
  case ExpressionRejectionReason::UndefOrPoisonOperand:
    return "undef_or_poison_operand";
  }
  llvm_unreachable("unknown Forge expression rejection reason");
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
