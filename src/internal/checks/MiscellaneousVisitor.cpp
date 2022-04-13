#include "internal/checks/MiscellaneousVisitor.h"

#include "shared/common/Common.h"
#include "shared/common/Config.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Basic/LLVM.h"
#include "clang/Frontend/CompilerInstance.h"

namespace ica {

MiscellaneousVisitor::MiscellaneousVisitor(clang::CompilerInstance & ci, const Config & checks)
    : Visitor(ci, checks)
{
    if (!isEnabled()) {
        return;
    }

    m_const_cast_member_id = getCustomDiagID(const_cast_member, "It is better to declare %0 as 'mutable' instead of 'const_cast'");
    m_useless_temp_id = getCustomDiagID(temporary_in_ctor, "%0 is not needed temporary, values can be written directly into field");
    m_static_in_header_id = getCustomDiagID(static_keyword, "global var in header");
    m_static_for_linkage_id = getCustomDiagID(static_keyword, "it's recommended to use anonymous namespace instead of 'static'");
}

bool MiscellaneousVisitor::VisitCXXConstCastExpr(clang::CXXConstCastExpr * cc_expr)
{
    if (!shouldProcessExpr(cc_expr, getSM()) || !getCheck(const_cast_member)) {
        return true;
    }

    auto cast_type = cc_expr->getType();
    if (cast_type.isConstQualified()) {
        return true;
    }

    auto member = clang::dyn_cast<clang::MemberExpr>(cc_expr->getSubExpr()->IgnoreImpCasts());
    if (member && clang::isa<clang::CXXThisExpr>(member->getBase())) {
        report(cc_expr->getExprLoc(), m_const_cast_member_id)
            .AddValue(member->getMemberDecl());
    }

    return true;
}


bool MiscellaneousVisitor::VisitCXXConstructorDecl(clang::CXXConstructorDecl * ctor_decl)
{
    if (!shouldProcessDecl(ctor_decl, getSM()) && ctor_decl->doesThisDeclarationHaveABody()) {
        return true;
    }

    m_ctor_body_stack.push_back(ctor_decl->getBody());
    return true;
}

bool MiscellaneousVisitor::VisitDeclRefExpr(clang::DeclRefExpr * decl_ref)
{
    if (!shouldProcessExpr(decl_ref, getSM()) && currCtor()) {
        return true;
    }

    removeAssigned(clang::dyn_cast<clang::VarDecl>(decl_ref->getDecl()));

    return true;
}

bool MiscellaneousVisitor::VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * op_call)
{
    if (!(shouldProcessExpr(op_call, getSM()) || currCtor())) {
        return true;
    }

    if (!op_call->isAssignmentOp()) {
        return true;
    }

    auto member = clang::dyn_cast<clang::MemberExpr>(op_call->getArg(0)->IgnoreParenCasts());
    if (!(member && clang::isa<clang::CXXThisExpr>(member->getBase()))) {
        return true;
    }

    auto rhs = clang::dyn_cast<clang::DeclRefExpr>(op_call->getArg(1)->IgnoreParenCasts());
    if (!rhs) {
        if (auto call = clang::dyn_cast<clang::CallExpr>(op_call->getArg(1)->IgnoreParenCasts()); call && call->getNumArgs() == 1) {
            auto func_decl = call->getDirectCallee();
            if (auto identifier = func_decl->getIdentifier()) {
                if (identifier->isStr("move") || identifier->isStr("forward")) {
                    rhs = clang::dyn_cast_or_null<clang::DeclRefExpr>(call->getArg(0));
                }
            }
        }
    }

    if (rhs) {
        auto field_decl = clang::dyn_cast<clang::FieldDecl>(member->getMemberDecl());
        auto var_decl = clang::dyn_cast<clang::VarDecl>(rhs->getDecl());
        if (field_decl && var_decl) {
            auto l_type = field_decl->getType()->getCanonicalTypeUnqualified();
            auto r_type = var_decl->getType()->getCanonicalTypeUnqualified();
            if (l_type == r_type) {
                addAssigned(var_decl);
            }
        }
    }

    return true;
}

bool MiscellaneousVisitor::VisitVarDecl(clang::VarDecl * var_decl)
{
    if (!shouldProcessDecl(var_decl, getSM()) || !getCheck(static_keyword)) {
        return true;
    }

    if (!m_comp_stmt_stack.empty() || var_decl->getType().isConstQualified()) {
        return true;
    }

    if (var_decl->hasGlobalStorage() && !var_decl->isTemplated()) {
        auto file_name = getSM().getFilename(var_decl->getLocation());
        if (file_name.endswith(".h") || file_name.endswith(".hpp")) {
            report(var_decl->getLocation(), m_static_in_header_id);
        }
    }
    return true;
}

bool MiscellaneousVisitor::VisitFunctionDecl(clang::FunctionDecl * func_decl)
{
    if (!shouldProcessDecl(func_decl, getSM()) || !getCheck(static_keyword)) {
        return true;
    }

    func_decl = func_decl->getCanonicalDecl();
    if (func_decl->isStatic() && !clang::isa<clang::CXXMethodDecl>(func_decl)) {
        report(func_decl->getLocation(), m_static_for_linkage_id);
    }

    return true;
}

bool MiscellaneousVisitor::VisitCompoundStmt(clang::CompoundStmt * comp_stmt)
{
    if (!shouldProcessStmt(comp_stmt, getSM()) || !getCheck(static_keyword)) {
        return true;
    }

    m_comp_stmt_stack.push_back(comp_stmt);
    return true;
}

bool MiscellaneousVisitor::dataTraverseStmtPost(clang::Stmt * stmt)
{
    if (stmt == currCtor() && getCheck(temporary_in_ctor)) {
        m_ctor_body_stack.pop_back();
        for (auto & [var_decl, _] : m_last_assigned) {
            report(var_decl->getLocation(), m_useless_temp_id)
                .AddValue(var_decl);
        }
        m_last_assigned.clear();
    }
    if (auto comp_stmt = clang::dyn_cast<clang::CompoundStmt>(stmt)) {
        if (!m_comp_stmt_stack.empty() && m_comp_stmt_stack.back() == comp_stmt) {
            m_comp_stmt_stack.pop_back();
        }
    }

    return true;
}


void MiscellaneousVisitor::addAssigned(const clang::VarDecl * var_decl)
{
    if (var_decl->isLocalVarDecl()) {
        m_last_assigned.emplace(var_decl, 1);
    }
}

void MiscellaneousVisitor::removeAssigned(const clang::VarDecl * var_decl)
{
    auto it = m_last_assigned.find(var_decl);
    if (it != m_last_assigned.end()) {
        if (it->second != 0) {
            it->second--;
        } else {
            m_last_assigned.erase(it);
        }
    }
}

const clang::Stmt * MiscellaneousVisitor::currCtor() const
{
    return m_ctor_body_stack.empty() ? nullptr : m_ctor_body_stack.back();
}

} // namespace ica
