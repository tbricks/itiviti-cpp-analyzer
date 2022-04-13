#pragma once

#include "shared/common/Config.h"
#include "shared/common/DiagnosticsBuilder.h"
#include "shared/common/Visitor.h"

#include "clang/AST/Decl.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Frontend/CompilerInstance.h"

#include <unordered_map>

namespace ica {

class MiscellaneousVisitor : public Visitor<MiscellaneousVisitor>
{
    static constexpr auto * const_cast_member = "const-cast-member";
    static constexpr auto * temporary_in_ctor = "temporary-in-ctor";
    static constexpr auto * static_keyword = "static-keyword";

public:
    static constexpr auto check_names = make_check_names(const_cast_member, temporary_in_ctor, static_keyword);

    MiscellaneousVisitor(clang::CompilerInstance & ci, const Config & checks);

    bool VisitCXXConstCastExpr(clang::CXXConstCastExpr * cc_expr);

    bool VisitCXXConstructorDecl(clang::CXXConstructorDecl * ctor_decl);
    bool VisitDeclRefExpr(clang::DeclRefExpr * decl_ref);
    bool VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * op_call);

    bool VisitVarDecl(clang::VarDecl * var_decl);
    bool VisitFunctionDecl(clang::FunctionDecl * func_decl);
    bool VisitCompoundStmt(clang::CompoundStmt * comp_stmt);

    bool dataTraverseStmtPost(clang::Stmt * stmt);

    void printDiagnostic(clang::ASTContext &) {}
    void clear() {}

private:
    void addAssigned(const clang::VarDecl * var_decl);
    void removeAssigned(const clang::VarDecl * var_decl);

    const clang::Stmt * currCtor() const;

private:
    std::unordered_map<const clang::VarDecl *, uint> m_last_assigned;
    std::vector<const clang::Stmt *> m_ctor_body_stack;
    std::vector<const clang::CompoundStmt *> m_comp_stmt_stack;

    DiagnosticID m_const_cast_member_id = 0;
    DiagnosticID m_useless_temp_id = 0;
    DiagnosticID m_static_in_header_id = 0;
    DiagnosticID m_static_for_linkage_id = 0;
};

} // namespace ica
