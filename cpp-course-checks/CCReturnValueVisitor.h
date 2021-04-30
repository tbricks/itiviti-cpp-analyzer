#pragma once

#include "ICAConfig.h"
#include "ICADiagnosticsBuilder.h"
#include "ICAVisitor.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/Frontend/CompilerInstance.h>

class CCReturnValueVisitor : public ICAVisitor<CCReturnValueVisitor>
{
    static constexpr auto * return_value_type = "return-value-type";

public:
    static constexpr auto check_names = make_check_names(return_value_type);

public:
    CCReturnValueVisitor(clang::CompilerInstance & ci, const ICAConfig & checks);

    bool VisitFunctionDecl(clang::FunctionDecl * func_decl);
    bool VisitReturnStmt(clang::ReturnStmt * return_stmt);

    bool dataTraverseStmtPost(clang::Stmt * stmt);

    void printDiagnostic(clang::ASTContext &) {}
    void clear() {}

private:
    struct CheckedFunc
    {
        CheckedFunc(const clang::FunctionDecl * func_decl)
            : func_decl(func_decl)
        {}

        void and_return(bool returns_this) noexcept {
            if (!always_returns_this.has_value()) {
                always_returns_this.emplace(returns_this);
            } else {
                *always_returns_this &= returns_this;
            }
        }

        bool returns_this() const noexcept { return always_returns_this.has_value() && *always_returns_this; }

        const clang::FunctionDecl * func_decl;
        std::optional<bool> always_returns_this;
    };

    std::vector<CheckedFunc> m_comp_stmt_stack;
    DiagnosticID m_return_const_value_id = 0;
    DiagnosticID m_return_self_copy_id = 0;
};