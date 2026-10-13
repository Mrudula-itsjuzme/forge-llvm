#pragma once

#include "llvm/IR/PassManager.h"

namespace forge {

class LocalCSEPass : public llvm::PassInfoMixin<LocalCSEPass> {
public:
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &AM);
};

class GlobalCSEPass : public llvm::PassInfoMixin<GlobalCSEPass> {
public:
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &AM);
};

class LoopInvariantCodeMotionPass
    : public llvm::PassInfoMixin<LoopInvariantCodeMotionPass> {
public:
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &AM);
};

} // namespace forge

