#include "forge/Expression.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static cl::OptionCategory ForgeCategory("forge-analyze options");
static cl::opt<std::string> Input(cl::Positional, cl::Required,
                                  cl::desc("<input LLVM IR or bitcode>"),
                                  cl::cat(ForgeCategory));
static cl::opt<std::string> Pipeline(
    "pipeline", cl::init("forge-local-cse,forge-global-cse,forge-licm"),
    cl::desc("Pipeline to include in the report"), cl::cat(ForgeCategory));

int main(int argc, char **argv) {
  cl::HideUnrelatedOptions(ForgeCategory);
  cl::ParseCommandLineOptions(argc, argv, "Forge IR analysis report\n");

  LLVMContext Context;
  SMDiagnostic Diagnostic;
  std::unique_ptr<Module> M = parseIRFile(Input, Diagnostic, Context);
  if (!M) {
    Diagnostic.print(argv[0], errs());
    return 1;
  }

  uint64_t Functions = 0;
  uint64_t Blocks = 0;
  uint64_t Instructions = 0;
  uint64_t Eligible = 0;
  uint64_t LocalOpportunities = 0;

  for (Function &F : *M) {
    if (F.isDeclaration())
      continue;
    ++Functions;
    Blocks += F.size();
    for (BasicBlock &BB : F) {
      DenseMap<forge::ExpressionKey, SmallVector<BinaryOperator *, 2>,
               forge::ExpressionKeyInfo>
          Available;
      for (Instruction &I : BB) {
        ++Instructions;
        auto *BO = dyn_cast<BinaryOperator>(&I);
        if (!BO || !forge::isEligibleIntegerExpression(*BO))
          continue;
        ++Eligible;
        auto &Candidates = Available[forge::makeExpressionKey(*BO)];
        bool Found = false;
        for (BinaryOperator *Candidate : Candidates)
          if (forge::areEquivalentExpressions(*BO, *Candidate)) {
            Found = true;
            break;
          }
        if (Found)
          ++LocalOpportunities;
        else
          Candidates.push_back(BO);
      }
    }
  }

  json::Object Report{{"module", M->getModuleIdentifier()},
                      {"pipeline", Pipeline.getValue()},
                      {"functions", Functions},
                      {"basic_blocks", Blocks},
                      {"instructions", Instructions},
                      {"eligible_integer_expressions", Eligible},
                      {"local_cse_opportunities", LocalOpportunities}};
  outs() << formatv("{0:2}\n", json::Value(std::move(Report)));
  return 0;
}

