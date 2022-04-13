#pragma once

#include "shared/common/Config.h"
#include "shared/common/DiagnosticsBuilder.h"
#include "shared/common/Visitor.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Frontend/CompilerInstance.h"

#include <unordered_set>

namespace ica {

class InitMembersVisitor : public Visitor<InitMembersVisitor>
{
    static constexpr auto * init_members = "init-members";
public:
    static constexpr auto check_names = make_check_names(init_members);

    InitMembersVisitor(clang::CompilerInstance & ci, const Config & checks);

    bool VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * op_call);
    bool VisitBinaryOperator(clang::BinaryOperator * op_call);
    bool VisitMemberExpr(clang::MemberExpr * member);
    bool VisitCXXConstructorDecl(clang::CXXConstructorDecl * ctor_decl);

    bool dataTraverseStmtPost(clang::Stmt * stmt);

    void printDiagnostic(clang::ASTContext &) {}
    void clear() {};
private:
    void addToReport(const clang::MemberExpr * member);
    void foundMember(const clang::MemberExpr * member);

    void checkAssignment(const clang::Expr * lhs, const clang::Expr * rhs);

private:
    std::unordered_set<const clang::MemberExpr *> m_met_members;
    std::unordered_set<const clang::MemberExpr *> m_to_report;
    const clang::Stmt * m_curr_ctor_body = nullptr;
    DiagnosticID m_init_in_body_id = 0;
};

} // namespace ica
