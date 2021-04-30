#pragma once

#include "ICAVisitor.h"

#include <string>
#include <utility>
#include <tuple>
#include <unordered_map>

class ICAFindEmplaceVisitor : public ICAVisitor<ICAFindEmplaceVisitor>
{
    using VarAndCallExpr = std::pair<const clang::VarDecl *, const clang::CallExpr *>;
    using VarAndCallExprs = std::vector<VarAndCallExpr>;

    using ContainerKeyFind = std::tuple<const clang::Expr *, const clang::Expr *, const clang::Expr *>;
    using IteratorsFind = std::map<const clang::VarDecl *, ContainerKeyFind>;

    static constexpr inline auto * find_emplace = "find-emplace";

public:
    explicit ICAFindEmplaceVisitor(clang::CompilerInstance & ci, const ICAConfig & config);

public:
    static constexpr inline auto check_names = make_check_names(find_emplace);

public:
    bool dataTraverseStmtPre(clang::Stmt * stmt);
    bool dataTraverseStmtPost(clang::Stmt * stmt);

    bool VisitVarDecl(clang::VarDecl * decl);
    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * expr);
    bool VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * expr);

    void clear();
    void printDiagnostic(clang::ASTContext & context) { }

private:
    const clang::VarDecl * getSameAsFindVar(const clang::CallExpr * call, const clang::Expr * cont, const clang::Expr * key);
    bool isOperatorSquareBracketsInsideIf(const clang::Expr * expr);

    void reportCompoundStmt();

private:
    DiagnosticID m_warn_id = 0;
    DiagnosticID m_note_id = 0;

    VarAndCallExprs m_calls;
    clang::CompoundStmt * m_curr_stmt;
    std::unordered_map<clang::CompoundStmt *, clang::CompoundStmt *>  m_parent_of;
    std::unordered_map<clang::CompoundStmt *, IteratorsFind> m_stmt_iterators;
};
