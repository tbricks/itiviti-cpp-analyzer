#pragma once

#include "shared/common/Visitor.h"
#include "shared/common/Common.h"

#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Type.h"

#include <string>
#include <utility>
#include <tuple>
#include <unordered_map>
#include <optional>

namespace ica {

class FindEmplaceVisitor : public Visitor<FindEmplaceVisitor>
{
    using VarAndCallExpr = std::pair<const clang::VarDecl *, const clang::CallExpr *>;
    using VarAndCallExprs = std::vector<VarAndCallExpr>;

    using ContainerKeyFind = std::tuple<const clang::Expr *, const clang::Expr *, const clang::Expr *>;
    using IteratorsFind = std::map<const clang::VarDecl *, ContainerKeyFind>;

    static constexpr auto * find_emplace = "find-emplace";
    static constexpr auto * try_emplace = "try_emplace-instead-emplace";

public:
    explicit FindEmplaceVisitor(clang::CompilerInstance & ci, const Config & config);

public:
    static constexpr inline auto check_names = make_check_names(find_emplace, try_emplace);

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
    bool isKeyTypeCopyableOrMovable(const clang::CXXRecordDecl * decl);
    bool verifyTryEmplace(const clang::CXXMemberCallExpr * expr,
                          const std::string_view func_name,
                          const clang::QualType & type);
    void reportCompoundStmt();
    void makeTryEmplaceReport(const clang::CXXMemberCallExpr * memberCallExpr);

private:
    struct GetKeyTypeFunctor
    {
        std::optional<clang::QualType> operator() (const clang::CXXRecordDecl *) const;
    };
    DiagnosticID m_find_emplace_warn_id = 0;
    DiagnosticID m_try_emplace_warn_id = 0;
    DiagnosticID m_note_id = 0;
    VarAndCallExprs m_calls;
    clang::CompoundStmt * m_curr_stmt;
    std::unordered_map<clang::CompoundStmt *, clang::CompoundStmt *>  m_parent_of;
    std::unordered_map<clang::CompoundStmt *, IteratorsFind> m_stmt_iterators;
    MemorizingFunctor<GetKeyTypeFunctor> m_mem_func_get_key_type;
};

} // namespace ica
