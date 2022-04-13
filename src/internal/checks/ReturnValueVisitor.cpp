#include "internal/checks/ReturnValueVisitor.h"

#include "shared/common/Common.h"
#include "shared/common/Config.h"

#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/OperationKinds.h"

namespace ica {

ReturnValueVisitor::ReturnValueVisitor(clang::CompilerInstance & ci, const Config & checks)
    : Visitor(ci, checks)
{
    if (!isEnabled()) {
        return;
    }

    m_return_const_value_id = getCustomDiagID(return_value_type, "%0 returns const value. Consider using %1 instead");
    m_return_self_copy_id = getCustomDiagID(return_value_type, "%0 always returns copy of '*this', consider returning %1");
}

bool ReturnValueVisitor::VisitFunctionDecl(clang::FunctionDecl * func_decl)
{
    if (!shouldProcessDecl(func_decl, getSM())) {
        return true;
    }

    if (func_decl->getReturnType()->isVoidType()) {
        return true;
    }

    if (func_decl->getTemplateInstantiationPattern()) {
        return true;
    }

    auto ret_type = func_decl->getReturnType();

    if (ret_type->isReferenceType() || ret_type->isPointerType()) {
        return true;
    }

    if (ret_type.isConstQualified()) {
        auto replace_type = "'" + removeClassPrefix(ret_type.getAsString()) + " &'";
        if (ret_type->isBuiltinType()) {
            ret_type = ret_type.getUnqualifiedType();
            replace_type += " or '" + ret_type.getAsString() + "'";
        }
        report(func_decl->getLocation(), m_return_const_value_id)
            .AddValue(func_decl)
            .AddValue(replace_type);
    }

    if (!func_decl->doesThisDeclarationHaveABody()) {
        return true;
    }

    m_comp_stmt_stack.emplace_back(func_decl);
    return true;
}

bool ReturnValueVisitor::VisitReturnStmt(clang::ReturnStmt * return_stmt)
{
    if (!shouldProcessStmt(return_stmt, getSM()) || m_comp_stmt_stack.empty()) {
        return true;
    }

    bool returns_this = false;
    auto returned_expr = return_stmt->getRetValue();
    if (auto construct_expr = clang::dyn_cast<clang::CXXConstructExpr>(returned_expr)) {
        if (construct_expr->getConstructor()->isCopyConstructor()) {
            returned_expr = construct_expr->getArg(0)->IgnoreParenCasts();
        }
    }
    if (auto unary_oper = clang::dyn_cast<clang::UnaryOperator>(returned_expr)) {
        if (unary_oper->getOpcode() == clang::UO_Deref) {
            returns_this = clang::isa<clang::CXXThisExpr>(unary_oper->getSubExpr());
        }
    }
    m_comp_stmt_stack.back().and_return(returns_this);

    return true;
}

bool ReturnValueVisitor::dataTraverseStmtPost(clang::Stmt * stmt)
{
    if (!m_comp_stmt_stack.empty()) {
        auto & top = m_comp_stmt_stack.back();
        if (top.func_decl->getBody() == stmt) {
            if (top.returns_this()) {
                auto ret_type = top.func_decl->getReturnType().getUnqualifiedType();
                auto replace_type = "'const " + removeClassPrefix(ret_type.getAsString()) + " &'";
                report(top.func_decl->getLocation(), m_return_self_copy_id)
                    .AddValue(top.func_decl)
                    .AddValue(replace_type);
            }
            m_comp_stmt_stack.pop_back();
        }
    }
    return true;
}

} // namespace ica
