#pragma once

#include "ICAVisitor.h"

#include <string>
#include <utility>
#include <tuple>
#include <vector>

class ICAEraseInLoopVisitor : public ICAVisitor<ICAEraseInLoopVisitor>
{
    using ForAndVar = std::pair<const clang::ForStmt *, const clang::VarDecl *>;

    static constexpr inline auto * find_emplace = "erase-in-loop";

public:
    explicit ICAEraseInLoopVisitor(clang::CompilerInstance & ci, const ICAConfig & config);

public:
    static constexpr inline auto check_names = make_check_names(find_emplace);

public:
    bool VisitForStmt(clang::ForStmt * expr);
    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * expr);

    bool dataTraverseStmtPre(clang::Stmt * s);
    bool dataTraverseStmtPost(clang::Stmt * s);

    void clear();
    void printDiagnostic(clang::ASTContext & context) { }

private:
    const clang::ForStmt * findForStrmForVar(const clang::VarDecl * var_decl) const;
    const clang::Stmt * getParentOfCurrentStmt() const;

    void reportInvalidErase(
            const clang::CXXMemberCallExpr * call_expr,
            const clang::ForStmt * for_stmt,
            const clang::VarDecl * var_decl);

private:
    DiagnosticID m_warn_id = 0;
    DiagnosticID m_note_id = 0;

    std::vector<ForAndVar> m_stack;
    std::vector<const clang::Stmt *> m_parents;
};
