#include "shared/checks/EraseInLoopVisitor.h"
#include "shared/common/Common.h"
#include "shared/common/DiagnosticsBuilder.h"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace ica {

EraseInLoopVisitor::EraseInLoopVisitor(clang::CompilerInstance & ci, const Config & config)
    : Visitor(ci, config)
{
    if (!isEnabled()) return;

    m_warn_id = getCustomDiagID(find_emplace,
            "erase() invalidates %q0, which is incremented in a for loop");

    m_note_id = getCustomDiagID(clang::DiagnosticIDs::Note,
            "increment called here");

    m_stack.reserve(16);
    m_parents.reserve(256);
}


namespace {

bool hasReturnOrBreakChild(const clang::Stmt * stmt)
{
    if (!stmt) {
        return false;
    }

    const auto is_return_or_break = [] (const auto * stmt) {
        const auto stmt_class = stmt
            ? stmt->getStmtClass()
            : clang::Stmt::NoStmtClass;

        switch (stmt_class) {
        case clang::Stmt::BreakStmtClass:
        case clang::Stmt::ReturnStmtClass:
            return true;
        default:
            return false;
        }
    };

    return std::any_of(stmt->child_begin(), stmt->child_end(), is_return_or_break);
}


const clang::VarDecl * getVarIfDeclRefExpr(const clang::Expr * expr)
{
    if (expr) {
        expr = expr->IgnoreParenImpCasts();
    }

    if (auto * decl_ref = clang::dyn_cast_or_null<clang::DeclRefExpr>(expr); decl_ref) {
        return clang::dyn_cast<clang::VarDecl>(decl_ref->getDecl());
    }
    return nullptr;
}

const clang::VarDecl * getVarIfIncIsPreOrPostIncrement(const clang::Expr * expr)
{
    if (expr) {
        expr = expr->IgnoreParenImpCasts();
    }

    /*
    if (auto * op = clang::dyn_cast_or_null<clang::UnaryOperator>(expr); op && op->isIncrementOp()) {
        return getVarIfDeclRefExpr(op->getSubExpr());
    }
    */
    if (auto * op = clang::dyn_cast_or_null<clang::CXXOperatorCallExpr>(expr); op) {
        if (op->getOperator() == clang::OO_PlusPlus) {
            return getVarIfDeclRefExpr(op->getArg(0));
        }
    }

    return nullptr;
}

const clang::VarDecl * getVarIfDeclRefOrConstructExpr(const clang::Expr * expr)
{
    if (auto * ctor_expr = clang::dyn_cast_or_null<clang::CXXConstructExpr>(expr); ctor_expr) {
        if (ctor_expr->getNumArgs() == 1) {
            return getVarIfDeclRefExpr(ctor_expr->getArg(0));
        }
    }
    return getVarIfDeclRefExpr(expr);
}

} // namespace anonymous

bool EraseInLoopVisitor::VisitForStmt(clang::ForStmt * for_stmt)
{
    if (!shouldProcessStmt(for_stmt, getSM())) {
        return true;
    }

    if (auto * var_decl = getVarIfIncIsPreOrPostIncrement(for_stmt->getInc()); var_decl) {
        m_stack.emplace_back(for_stmt, var_decl);
    }
    return true;
}

bool EraseInLoopVisitor::dataTraverseStmtPre(clang::Stmt * s)
{
    m_parents.push_back(s);

    return true;
}

bool EraseInLoopVisitor::dataTraverseStmtPost(clang::Stmt * s)
{
    m_parents.pop_back();

    if (!m_stack.empty() && m_stack.back().first == s) {
        m_stack.pop_back();
    }

    return true;
}

const clang::ForStmt * EraseInLoopVisitor::findForStrmForVar(const clang::VarDecl * var_decl) const
{
    assert(var_decl);

    const auto same_var = [var_decl] (const auto & elem) {
        return elem.second == var_decl;
    };

    const auto it = std::find_if(m_stack.rbegin(), m_stack.rend(), same_var);
    if (it != m_stack.rend()) {
        return it->first;
    }
    return nullptr;
}

void EraseInLoopVisitor::reportInvalidErase(
        const clang::CXXMemberCallExpr * call_expr,
        const clang::ForStmt * for_stmt,
        const clang::VarDecl * var_decl)
{
    report(call_expr->getExprLoc(), m_warn_id)
        .AddValue(var_decl);

    report(for_stmt->getInc()->getExprLoc(), m_note_id);
}

const clang::Stmt * EraseInLoopVisitor::getParentOfCurrentStmt() const
{
    if (m_parents.size() >= 2) {
        return *(m_parents.end() - 2);
    }
    return nullptr;
}

bool EraseInLoopVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * call_expr)
{
    if (!shouldProcessExpr(call_expr, getSM())) {
        return true;
    }

    auto * method_decl = call_expr->getMethodDecl();
    if (!method_decl) {
        return true;
    }

    const auto func_name = method_decl->getNameInfo().getAsString();
    const auto obj_expr = call_expr->getImplicitObjectArgument();

    if (!obj_expr) {
        return true;
    }

    if (func_name == "erase" && call_expr->getNumArgs() == 1) {
        if (auto * var_decl = getVarIfDeclRefOrConstructExpr(call_expr->getArg(0)); var_decl) {
            if (auto * for_stmt = findForStrmForVar(var_decl); for_stmt) {
                if (!hasReturnOrBreakChild(getParentOfCurrentStmt())) {
                    reportInvalidErase(call_expr, for_stmt, var_decl);
                }
            }
        }
    }

    return true;
}

void EraseInLoopVisitor::clear()
{
    resetContext();
    m_stack.clear();
}

} // namespace ica
