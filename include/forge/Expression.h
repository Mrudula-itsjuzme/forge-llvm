#pragma once

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instructions.h"

namespace forge {

enum class ExpressionRejectionReason {
  Eligible,
  NonIntegerType,
  UnsupportedOrSemantics,
  UndefOrPoisonOperand,
};

struct ExpressionKey {
  unsigned Opcode;
  llvm::Type *ResultType;
  llvm::Value *LHS;
  llvm::Value *RHS;

  bool operator==(const ExpressionKey &Other) const;
};

struct ExpressionKeyInfo {
  static ExpressionKey getEmptyKey();
  static ExpressionKey getTombstoneKey();
  static unsigned getHashValue(const ExpressionKey &Key);
  static bool isEqual(const ExpressionKey &A, const ExpressionKey &B);
};

bool isEligibleIntegerExpression(const llvm::BinaryOperator &BO);
ExpressionRejectionReason
classifyIntegerExpression(const llvm::BinaryOperator &BO);
llvm::StringRef rejectionReasonName(ExpressionRejectionReason Reason);
ExpressionKey makeExpressionKey(const llvm::BinaryOperator &BO);
bool haveSameSemanticFlags(const llvm::BinaryOperator &A,
                           const llvm::BinaryOperator &B);
bool areEquivalentExpressions(const llvm::BinaryOperator &A,
                              const llvm::BinaryOperator &B);

} // namespace forge
