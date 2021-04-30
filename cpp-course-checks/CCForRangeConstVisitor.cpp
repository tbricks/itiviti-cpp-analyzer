#include "CCForRangeConstVisitor.h"
#include "ICACommon.h"
#include "ICAConfig.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/StmtCXX.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/OperatorKinds.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Frontend/CompilerInstance.h>

#include <algorithm>
#include <numeric>
#include <string_view>
namespace {

bool isConstType(const clang::QualType & type)
{
    auto derefed = type.getNonReferenceType();
    return derefed->isPointerType() ? derefed->getPointeeType().isConstQualified() : derefed.isConstQualified();
}

bool isRefOrPtr(const clang::QualType & type)
{
    return type->isReferenceType() || type->isPointerType();
}

bool sameParams(const clang::FunctionDecl * l_func, const clang::FunctionDecl * r_func)
{
    return l_func->getNumParams() == r_func->getNumParams() &&
            std::equal(l_func->param_begin(), l_func->param_end(), r_func->param_begin(), [](const clang::ParmVarDecl * l_param, const clang::ParmVarDecl * r_param) {
                return l_param->getType() == r_param->getType();
            });
}

}

CCForRangeConstVisitor::CCForRangeConstVisitor(clang::CompilerInstance & ci, const ICAConfig & checks)
    : ICAVisitor(ci, checks)
{
    if (!isEnabled()) return;

    m_for_range_const_id = getCustomDiagID(for_range_const, "'const' should be specified explicitly for variable in for-range loop");
    m_const_param_id = getCustomDiagID(const_param, "%0 can have 'const' qualifier");
    m_const_rvalue_id = getCustomDiagID(const_param, "%0 is got as rvalue-reference, but never modified");
}

bool CCForRangeConstVisitor::VisitCXXForRangeStmt(clang::CXXForRangeStmt *for_stmt)
{
    if(!shouldProcessStmt(for_stmt, getSM())) {
        return true;
    }

    auto loop_var = for_stmt->getLoopVariable();
    if (!isConstType(loop_var->getType()) && isRefOrPtr(loop_var->getType())) {
        removeLoopVar(extractDeclRef(for_stmt->getRangeInit()));
    }

    if (isConstType(loop_var->getType()) || loop_var->getType()->isPointerType()) {
        return true; // ignore already const variables or pointers
    }

    if (auto decomp_decl = clang::dyn_cast<clang::DecompositionDecl>(loop_var)) {
        for(const auto binding : decomp_decl->bindings()) {
            m_loop_vars.insert(LoopVarEntry(for_stmt, binding));
        }
    } else {
        m_loop_vars.insert(LoopVarEntry(for_stmt, loop_var));
    }

    return true;
}

bool CCForRangeConstVisitor::VisitFunctionDecl(clang::FunctionDecl * func_decl)
{
    if (!shouldProcessDecl(func_decl, getSM())) {
        return true;
    }

    if (func_decl->isTemplated()) {
        auto [it, emplaced] = m_non_templated_params.try_emplace(func_decl);
        if (emplaced) {
            uint parm_num = 0;
            for (const auto * parm : func_decl->parameters()) {
                if (!parm->getType()->isInstantiationDependentType() && !parm->getType()->isPointerType()) {
                    it->second.push_back(parm_num);
                }
                ++parm_num;
            }
        }
        return true;
    }

    if (!func_decl->doesThisDeclarationHaveABody()) {
        return true;
    }

    if (auto method_decl = clang::dyn_cast<clang::CXXMethodDecl>(func_decl)) {
        if (method_decl->isVirtual()) {
            return true;
        }
    }

    std::function<bool(uint)> not_templated = [](auto) { return true; };
    if (auto it = m_non_templated_params.find(func_decl->getTemplateInstantiationPattern()); it != m_non_templated_params.end()) {
        const auto & line = it->second;
        not_templated = [expected_idx(0), &line](uint parm_num) mutable {
            bool res = false;
            if (expected_idx < line.size()) {
                res = line[expected_idx] == parm_num;
            }
            expected_idx += res;
            return res;
        };
    }

    std::set<const clang::ParmVarDecl *> ref_initializers;
    if (auto as_ctor_decl = clang::dyn_cast<clang::CXXConstructorDecl>(func_decl)) {
        for (auto init_expr : as_ctor_decl->inits()) {
            if (auto member = init_expr->getAnyMember()) {
                if (auto member_type = member->getType(); !isRefOrPtr(member_type) || isConstType(member_type)) {
                    continue;
                }
                if (auto decl_ref = extractDeclRef(init_expr->getInit())) {
                    ref_initializers.insert(clang::dyn_cast<clang::ParmVarDecl>(decl_ref->getDecl()));
                }
            }
        }
    }
    auto inits_ref = [&ref_initializers](const clang::ParmVarDecl * parm) {
        return ref_initializers.find(parm) != ref_initializers.end();
    };

    const auto * body = func_decl->getBody();
    uint parm_num = 0;
    for (const auto * parm : func_decl->parameters()) {
        if (not_templated(parm_num) && !isConstType(parm->getType()) && parm->getType()->isReferenceType() && !inits_ref(parm)) {
            m_loop_vars.insert(LoopVarEntry{body, parm});
        }
        ++parm_num;
    }

    return true;
}

bool CCForRangeConstVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *member_call)
{
    if (m_loop_vars.empty()) {
        return true;
    }

    auto method_decl = member_call->getMethodDecl();
    if (!method_decl) {
        return true;
    }

    if (method_decl->isConst() || hasConstOverload(method_decl)) {
        return true;
    }

    const auto caller = extractDeclRef(member_call->getImplicitObjectArgument());

    removeLoopVar(caller);
    return true;
}

bool CCForRangeConstVisitor::VisitCallExpr(clang::CallExpr *call)
{
    if (m_loop_vars.empty()) {
        return true;
    }

    if (auto callee = call->getDirectCallee()) {
        uint arg_num = 0;
        if (auto method = clang::dyn_cast<clang::CXXMethodDecl>(callee); method && clang::isa<clang::CXXOperatorCallExpr>(call)) {
            if (!hasConstOverload(method)) {
                removeLoopVar(extractDeclRef(call->getArg(arg_num)));
            }
            ++arg_num;
        }
        for (const auto * parm : callee->parameters()) {
            if (!isConstType(parm->getType()) && isRefOrPtr(parm->getType())) {
                removeLoopVar(extractDeclRef(call->getArg(arg_num)));
            }
            ++arg_num;
        }
        for (auto arg_it = call->arg_begin() + arg_num; arg_it != call->arg_end(); ++ arg_it) {
            if (isRefOrPtr((*arg_it)->getType())) {
                removeLoopVar(extractDeclRef(*arg_it));
            }
        }
    }

    return true;
}



bool CCForRangeConstVisitor::VisitBinaryOperator(clang::BinaryOperator * bin_op)
{
    if (m_loop_vars.empty()) {
        return true;
    }

    if (!bin_op->isAssignmentOp()) {
        return true;
    }

    auto l_arg = bin_op->getLHS();


    if (const auto loop_var_ref = extractDeclRef(l_arg)) {
        removeLoopVar(loop_var_ref);
    }
    return true;
}

bool CCForRangeConstVisitor::VisitUnaryOperator(clang::UnaryOperator *un_op)
{
    if (m_loop_vars.empty()) {
        return true;
    }

    if (un_op->isIncrementDecrementOp()) {
        removeLoopVar(clang::dyn_cast<clang::DeclRefExpr>(un_op->getSubExpr()));
    }

    return true;
}

bool CCForRangeConstVisitor::VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * op_call)
{
    if (m_loop_vars.empty()) {
        return true;
    }

    if (const auto * func_decl = op_call->getDirectCallee()) {
        if (const auto * method_decl = clang::dyn_cast<clang::CXXMethodDecl>(func_decl)) {
            if (!(method_decl->isConst() || hasConstOverload(method_decl))) {
                removeLoopVar(extractDeclRef(op_call->getArg(0)));
            }
        } else {
            if (auto lhs_type = func_decl->getParamDecl(0)->getType(); !isConstType(lhs_type) && isRefOrPtr(lhs_type)) {
                removeLoopVar(extractDeclRef(op_call->getArg(0)));
            }
        }
    }

    return true;
}

bool CCForRangeConstVisitor::dataTraverseStmtPost(clang::Stmt *stmt)
{
    if (auto [begin, end] = m_loop_vars.left.equal_range(stmt); begin != end && begin->get_left() == stmt) {
        if (const auto * for_stmt = clang::dyn_cast<clang::CXXForRangeStmt>(stmt)) {
            if (getCheck(for_range_const)) {
                report(for_stmt->getLoopVariable()->getTypeSpecStartLoc(), m_for_range_const_id);
            }
        } else if (getCheck(const_param)) {
            std::for_each(begin, end, [this](const auto & lve) {
                const clang::ValueDecl * parm = lve.get_right();
                report(parm->getBeginLoc(), parm->getType()->isRValueReferenceType() ? m_const_rvalue_id : m_const_param_id)
                    .AddValue(parm);
            });
        }
        m_loop_vars.left.erase(begin, end);
    }
    return true;
}

void CCForRangeConstVisitor::removeLoopVar(const clang::DeclRefExpr * loop_var)
{
    if (!loop_var) {
        return;
    }

    auto it = m_loop_vars.right.find(loop_var->getDecl());
    if (it != m_loop_vars.right.end()) {
        if (clang::isa<clang::CXXForRangeStmt>(it->get_left())) {
            m_loop_vars.left.erase(it->get_left()); // if one of binded values is modified, can't declare whole deomposition as 'const'
        } else {
            m_loop_vars.right.erase(it);
        }
    }
}

bool CCForRangeConstVisitor::hasConstOverload(const clang::CXXMethodDecl * method_decl)
{
    auto [cached_it, emplaced] = m_has_const_overload.try_emplace(method_decl, false);
    if (!emplaced) {
        return cached_it->second;
    }
    if (method_decl->isConst()) {
        return cached_it->second = true;
    }
    clang::CXXRecordDecl::method_range methods = method_decl->getParent()->methods();
    const clang::FunctionDecl * searched_func = method_decl;

    if (method_decl->isTemplateInstantiation()) {
        searched_func = method_decl->getTemplateInstantiationPattern();
        auto template_pattern = method_decl->getParent()->getTemplateInstantiationPattern();
        if (template_pattern) {
            methods = template_pattern->methods();
        }
    }

    auto accessible = [](const clang::CXXMethodDecl * other_func) {
        return other_func && other_func->isConst() && other_func->getAccess() == clang::AS_public;
    };
    auto same_params = [searched_func](const clang::FunctionDecl * other_func) { return sameParams(searched_func, other_func); };

    std::function<bool(const clang::CXXMethodDecl *)> same_name;
    if (auto identifier = searched_func->getIdentifier()) {
        same_name = [name(identifier->getName())](const auto * other_func) {
            if (auto identifier = other_func->getIdentifier()) {
                return identifier->getName() == name;
            }
            return false;
        };
    } else if (auto conversion = clang::dyn_cast<clang::CXXConversionDecl>(searched_func)) {
        same_name = [type(conversion->getConversionType())](const auto * other_func) {
            if (auto conversion = clang::dyn_cast<clang::CXXConversionDecl>(other_func)) {
                return conversion->getConversionType() == type;
            }
            return false;
        };
    } else if (searched_func->isOverloadedOperator()) {
        same_name = [op_kind(searched_func->getOverloadedOperator())](const auto * other_func) {
            return other_func->isOverloadedOperator() && other_func->getOverloadedOperator() == op_kind;
        };
    } else {
        same_name = [](const auto *) {
            return false;
        };
    }

    auto same_but_const = [&accessible, &same_name, &same_params](const clang::CXXMethodDecl * other_func) {
        return accessible(other_func) && same_name(other_func) && same_params(other_func);
    };

    return cached_it->second = std::any_of(methods.begin(), methods.end(), same_but_const);
}
