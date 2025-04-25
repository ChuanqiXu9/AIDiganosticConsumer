#include "AIDiagnosticConsumer.h"
#include "clang/Frontend/ChainedDiagnosticConsumer.h"
#include "clang/Frontend/CompilerInstance.h"

#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include <clang/AST/ASTConsumer.h>
#include <memory>

namespace clang {

class AIDiagAssitedAction : public PluginASTAction {
  std::string OutputFile;
  bool OnlyMacroInfo = false;

public:
  AIDiagAssitedAction(StringRef OutputFile) : OutputFile(OutputFile) {}
  AIDiagAssitedAction() = default;

  ActionType getActionType() override { return AddAfterMainAction; }

  // Required but we don't intend to consume any AST specificly.
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef InFile) override {
    // Slighly hack. The API expects us to create a new AST consumer but we hacked the compiler Instance
    // to create a diagnostic consumer. Maybe we can add some new abstractions here.
    auto &Diags = CI.getDiagnostics();
    auto AIDiagConsumer = createAIDiagnosticConsumer(CI);
    if (Diags.ownsClient()) {
        Diags.setClient(
            new ChainedDiagnosticConsumer(Diags.takeClient(), std::move(AIDiagConsumer)));
    } else {
        Diags.setClient(
            new ChainedDiagnosticConsumer(Diags.getClient(), std::move(AIDiagConsumer)));
    }

    // Unused ASTConsumer.
    return std::make_unique<ASTConsumer>();
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &arg) override {
      return true;
  }
};

} // namespace clang

static clang::FrontendPluginRegistry::Add<clang::AIDiagAssitedAction>
    X("AIDiag", "Use AI to translate and explain the diaanostic messages");
