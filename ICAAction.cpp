#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/Path.h"

#include "ICAConfig.h"
#include "ICAConsumer.h"

#include <iostream>

class ICAAction : public clang::PluginASTAction
{
public:
    explicit ICAAction() = default;

    virtual std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance & ci, llvm::StringRef in_file) override
    {
        return std::make_unique<ICAConsumer>(ci, std::move(m_config));
    }

    virtual bool ParseArgs(const clang::CompilerInstance & ci, const std::vector<std::string> & args) override
    {
        if (auto error = m_config.parse(args); error) {
            llvm::outs() << "Error while parsing ICA args: " << *error << '\n';
            return false;
        }

        return true;
    }

    virtual clang::PluginASTAction::ActionType getActionType() override
    {
        return AddBeforeMainAction;
    }

private:
    ICAConfig m_config;
};

static clang::FrontendPluginRegistry::Add<ICAAction> X("ica-plugin", "ITIVITI cpp analyzer");
