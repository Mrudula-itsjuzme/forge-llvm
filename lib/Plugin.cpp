#include "forge/Passes.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ForgePass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "forge-local-cse" ||
                      Name == "forge-redundant-expr-elim") {
                    FPM.addPass(forge::LocalCSEPass());
                    return true;
                  }
                  if (Name == "forge-global-cse") {
                    FPM.addPass(forge::GlobalCSEPass());
                    return true;
                  }
                  if (Name == "forge-licm") {
                    FPM.addPass(forge::LoopInvariantCodeMotionPass());
                    return true;
                  }
                  return false;
                });
          }};
}

