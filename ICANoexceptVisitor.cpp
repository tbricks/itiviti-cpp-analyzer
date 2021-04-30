#include "ICANoexceptVisitor.h"
#include "ICACommon.h"
#include "ICADiagnosticsBuilder.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/ExceptionSpecificationType.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/OperatorKinds.h>
#include <functional>

ICANoexceptVisitor::ICANoexceptVisitor(clang::CompilerInstance & ci, const ICAConfig & config)
    : ICAVisitor(ci, config)
{
    if (!isEnabled()) {
        return;
    }

    m_warn_id = getCustomDiagID(noexcept_check, "there is call of non-noexcept function within noexcept-specified");
    m_note_id = getCustomDiagID(clang::DiagnosticIDs::Note, "non-noexcept function call is here");
}

namespace {

bool isNoexcept(const clang::FunctionDecl * func_decl)
{
    auto ex_spec = func_decl->getExceptionSpecType();
    return  ex_spec == clang::EST_DynamicNone ||
            ex_spec == clang::EST_NoThrow ||
            ex_spec == clang::EST_BasicNoexcept ||
            ex_spec == clang::EST_DependentNoexcept ||
            ex_spec >= clang::EST_NoexceptTrue; // NoexceptTrue < Unevaluated < Uninstantiated < Unparsed. We assume that all those will be evaluated to noexcept(true)
}

bool isAssert(const clang::FunctionDecl * func_decl)
{
    return func_decl->getNameAsString() == "__assert_fail";
}

bool shouldReport(const clang::FunctionDecl * func_decl)
{
    if (!func_decl) {
        return false;
    }

    return !func_decl->hasBody() && !(isAssert(func_decl) || isNoexcept(func_decl));
}

bool shouldReport(const clang::CXXMethodDecl * method_decl)
{
    if (!method_decl) {
        return false;
    }
    return shouldReport(clang::dyn_cast<clang::FunctionDecl>(method_decl)) && !(method_decl->getExceptionSpecType() == clang::EST_None && method_decl->isDefaulted());
}

bool shouldReport(const clang::CXXConstructExpr * ctor_expr)
{
    if (!ctor_expr) {
        return false;
    }
    auto ctor_decl = ctor_expr->getConstructor();

    return shouldReport(ctor_decl) &&
        !(ctor_decl->getExceptionSpecType() == clang::EST_None && ctor_decl->isDefaulted());
}

bool shouldReport(const clang::CXXMemberCallExpr * member_call)
{
    if (!member_call) {
        return false;
    }
    return shouldReport(member_call->getMethodDecl());
}

bool shouldReport(const clang::CXXOperatorCallExpr * op_call)
{
    if (!op_call) {
        return false;
    }

    if (op_call->getOperator() == clang::OO_Call) {
        return shouldReport(op_call->getDirectCallee());
    } else if (op_call->isAssignmentOp()) {
        return shouldReport(clang::dyn_cast<clang::CXXMethodDecl>(op_call->getDirectCallee()));
    }
    return false;
}

bool shouldReport(const clang::CallExpr * call_expr)
{
    if (!call_expr) {
        return false;
    }

    if (auto member_call = clang::dyn_cast<clang::CXXMemberCallExpr>(call_expr)) {
        return shouldReport(member_call);
    } else if (auto op_call = clang::dyn_cast<clang::CXXOperatorCallExpr>(call_expr)) {
        return shouldReport(op_call);
    } else {
        return shouldReport(call_expr->getDirectCallee());
    }
}

bool shouldReport(const clang::Expr * expr)
{
    if (!expr) {
        return false;
    }

    return shouldReport(clang::dyn_cast<clang::CallExpr>(expr)) ||
            shouldReport(clang::dyn_cast<clang::CXXConstructExpr>(expr)) ||
            clang::dyn_cast<clang::CXXNewExpr>(expr) ||
            clang::dyn_cast<clang::CXXThrowExpr>(expr);
}
}

bool ICANoexceptVisitor::VisitFunctionDecl(clang::FunctionDecl * decl)
{
    if (!shouldProcessDecl(decl, getSM())) {
        return true;
    }

    if (!isNoexcept(decl) ||
        (decl->isTemplateInstantiation() &&
        (decl->getExceptionSpecType() >= clang::EST_Unevaluated || decl->getExceptionSpecType() == clang::EST_DependentNoexcept))) {
        return true;
    }

    if (auto definition = decl->getDefinition()) {
        std::function<bool(const clang::Stmt *)> check_children;
        check_children = [&check_children, &decl, this](const clang::Stmt * stmt) { // returns if should continue it's traverse
            if (!shouldProcessStmt(stmt, getSM())) {
                return true;
            }
            for (const auto sub_expr : stmt->children()) {
                if (!shouldProcessStmt(sub_expr, getSM())) {
                    continue;
                }
                if (shouldReport(clang::dyn_cast<clang::Expr>(sub_expr))) {
                    report(decl->getLocation(), m_warn_id);
                    report(sub_expr->getBeginLoc(), m_note_id);
                    return false;
                }
                if (!check_children(sub_expr)) {
                    return false;
                }
            }
            return true;
        };
        if (auto body = definition->getBody()) {
            check_children(body);
        }
    }

    return true;
}