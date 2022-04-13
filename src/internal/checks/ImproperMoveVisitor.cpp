#include "internal/checks/ImproperMoveVisitor.h"

#include "shared/common/Common.h"

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/OperationKinds.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/Specifiers.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <optional>
#include <string_view>

namespace  {

clang::QualType purifiedType(clang::QualType type)
{
    type = type.getNonReferenceType();
    if (type->isPointerType()) {
        type = type->getPointeeType();
    }
    return type->getCanonicalTypeUnqualified();
}

const clang::DeclRefExpr * getMethodCaller(const clang::CallExpr * member_call)
{
    const clang::Expr * res;
    if (const auto * method_call = clang::dyn_cast<clang::CXXMemberCallExpr>(member_call)) {
        res = method_call->getImplicitObjectArgument();
    } else if (const auto * op_call = clang::dyn_cast<clang::CXXOperatorCallExpr>(member_call)) {
        res = op_call->getArg(0);
    } else {
        llvm::errs() << "\nstrange member call\n";
        member_call->dumpColor();
        return nullptr;
    }
    return clang::dyn_cast<clang::DeclRefExpr>(res->IgnoreParenCasts());
}
} // namespace

namespace ica {

ImproperMoveVisitor::ImproperMoveVisitor(clang::CompilerInstance & ci, const Config & m_checks)
    : Visitor(ci, m_checks)
{
    if (!isEnabled()) {
        return;
    }

    m_move_lvalue_id = getCustomDiagID(improper_move, "'%0' got as non-const lvalue, moving from it may invalidate it's data");
    m_move_rvalue_id = getCustomDiagID(improper_move, "'std::move' argument is already an rvalue, no need to move it");
    m_move_to_const_id = getCustomDiagID(improper_move, "result of 'std::move' is implicitly cast to const reference");
    m_const_rvalue_id = getCustomDiagID(improper_move, "%0 is declared as const rvalue. Consider using either rvalue or const lvalue instead");
}

bool ImproperMoveVisitor::VisitFunctionDecl(clang::FunctionDecl * func_decl)
{
    if (!shouldProcessDecl(func_decl, getSM()) || !func_decl->doesThisDeclarationHaveABody()) {
        return true;
    }

    if (func_decl->isTemplated()) {
        auto [it, emplaced] = m_non_template_params.try_emplace(func_decl, false);
        if (emplaced) {
            uint parm_num = 0;
            for (const auto * parm : func_decl->parameters()) {
                if (!parm->getType()->isInstantiationDependentType()) {
                    it->second.push_back(parm_num);
                }
                ++parm_num;
            }
        }
    }

    bool is_private = false;
    if (auto method_decl = clang::dyn_cast<clang::CXXMethodDecl>(func_decl)) {
        is_private = method_decl->getAccess() == clang::AS_private;
    }

    std::function<bool(uint)> is_not_template = [](uint) { return true; };
    const auto temp_decl = func_decl->isTemplated() ? func_decl : func_decl->getTemplateInstantiationPattern();
    if (temp_decl) {
        if (auto nums_it = m_non_template_params.find(temp_decl); nums_it != m_non_template_params.end()) {
            is_not_template = [checked(0), size(nums_it->second.size()), curr_it(nums_it->second.cbegin())](uint parm_num) mutable {
                if (checked != size) {
                    ++checked;
                    if (*curr_it == parm_num) {
                        ++curr_it;
                        return true;
                    }
                }
                return false;
            };
        }
    }

    Parameters params;
    params.reserve(func_decl->getNumParams());

    std::transform(func_decl->param_begin(), func_decl->param_end(), std::back_inserter(params),
                   [this, is_private, parm_num(0), &is_not_template](const clang::ParmVarDecl * parm) mutable {
                       auto type = parm->getType();
                       const bool is_templated = !is_not_template(parm_num++);
                       const bool is_non_const_lvalue = !is_private && !type.isConstQualified() && type->isLValueReferenceType();
                       if (!is_templated && type->isRValueReferenceType() && type.getNonReferenceType().isConstQualified()) {
                           report(parm->getLocation(), m_const_rvalue_id)
                            .AddValue(parm);
                       }
                       return ParamTraits{parm, is_templated, is_non_const_lvalue};
                   });

    std::sort(params.begin(), params.end(),
              [less(std::less<const clang::ParmVarDecl *>{})](const auto & lhs, const auto & rhs) {
                  return less(lhs.parm_decl, rhs.parm_decl);
              });
    m_params_stack.emplace_back(func_decl->getBody(), std::move(params));
    return true;
}

bool ImproperMoveVisitor::VisitCallExpr(clang::CallExpr * call)
{
    if (!shouldProcessExpr(call, getSM())) {
        return true;
    }

    auto called_func = call->getDirectCallee();
    const clang::IdentifierInfo * identifier = nullptr;
    if (called_func) {
        identifier = called_func->getIdentifier();
    } else if (auto callee = call->getCallee()->IgnoreImpCasts()) {
        if (const auto * decl_ref = clang::dyn_cast<clang::DeclRefExpr>(callee)) {
            identifier = decl_ref->getDecl()->getIdentifier();
        } else if (const auto * unresolved = clang::dyn_cast<clang::UnresolvedLookupExpr>(callee)) {
            identifier = unresolved->getName().getAsIdentifierInfo();
        } else {
            return true;
        }
    }
    if (!(identifier && identifier->getName().equals("move"))) {
        return true;
    }

    if (m_last_const_cast) {
        if (purifiedType(m_last_const_cast->getType()) == purifiedType(call->getType())) {
            report(call->getExprLoc(), m_move_to_const_id);
        }
    }

    if (call->getNumArgs() < 1) {
        return true;
    }
    const auto * arg = call->getArg(0);

    if (const auto * arg_decl_ref = clang::dyn_cast<clang::DeclRefExpr>(arg)) {
        auto parm_decl = clang::dyn_cast<clang::ParmVarDecl>(arg_decl_ref->getDecl());
        if (parm_decl) {
            if (auto maybe_parm_traits = findParameter(parm_decl)) {
                if (maybe_parm_traits->is_non_const_lvalue) {
                    report(call->getExprLoc(), m_move_lvalue_id)
                        .AddValue(parm_decl->getName());
                }
            }
        }
    } else if (clang::isa<clang::MaterializeTemporaryExpr>(arg)) {
        report(call->getExprLoc(), m_move_rvalue_id);
    }

    arg = arg->IgnoreParenCasts();
    const auto * arg_decl_ref = clang::dyn_cast<clang::DeclRefExpr>(arg);
    if (!arg_decl_ref) {
        if (const auto * member_expr = clang::dyn_cast<clang::MemberExpr>(arg)) {
            arg_decl_ref = clang::dyn_cast<clang::DeclRefExpr>(member_expr->getBase());
            if (!arg_decl_ref) {
                if (const auto * member_call = clang::dyn_cast<clang::CallExpr>(member_expr->getBase())) {
                    if (auto type = member_call->getCallReturnType(getContext()); type->isReferenceType() || type->isPointerType()) {
                        arg_decl_ref = getMethodCaller(member_call);
                    }
                }
            }
        }
    }
    if (!arg_decl_ref) {
        if (const auto * dependent_member_expr = clang::dyn_cast<clang::CXXDependentScopeMemberExpr>(arg)) {
            arg_decl_ref = clang::dyn_cast<clang::DeclRefExpr>(dependent_member_expr->getBase()->IgnoreParenCasts());
        }
    }
    // if (arg_decl_ref) {
    //     if (const auto * parm_decl = clang::dyn_cast<clang::ParmVarDecl>(arg_decl_ref->getDecl())) {
    //         rvalueParamMoved(parm_decl);
    //     }
    // }

    return true;
}

bool ImproperMoveVisitor::dataTraverseStmtPre(clang::Stmt * stmt)
{
    if (auto imp_cast = clang::dyn_cast<clang::ImplicitCastExpr>(stmt)) {
        if (imp_cast->getCastKind() == clang::CK_NoOp) {
            m_last_const_cast = imp_cast;
        }
    }
    return true;
}

bool ImproperMoveVisitor::dataTraverseStmtPost(clang::Stmt *stmt)
{
    if (auto imp_cast = clang::dyn_cast<clang::ImplicitCastExpr>(stmt)) {
        if (imp_cast->getCastKind() == clang::CK_NoOp) {
            m_last_const_cast = nullptr;
        }
    }
    return true;
}

auto ImproperMoveVisitor::findParameter(const clang::ParmVarDecl *parm) -> MaybeParamTraits
{
    auto & curr_params = m_params_stack.back().second;
    auto found = std::lower_bound(curr_params.begin(), curr_params.end(), parm,
                            [less(std::less<const clang::ParmVarDecl *>{})](const ParamTraits & lhs, const clang::ParmVarDecl * rhs) {
                                return less(lhs.parm_decl, rhs);
                            });
    return found != curr_params.end() ? std::make_optional<ParamTraits>(*found) : std::nullopt;
}

} // namespace ica
