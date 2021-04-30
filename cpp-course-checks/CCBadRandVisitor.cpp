#include "CCBadRandVisitor.h"
#include "ICAChecks.h"
#include "ICACommon.h"
#include "ICAConfig.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/StmtCXX.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/OperatorKinds.h>
#include <clang/Frontend/CompilerInstance.h>

#include <algorithm>
#include <llvm/ADT/StringRef.h>
#include <string_view>

CCBadRandVisitor::CCBadRandVisitor(clang::CompilerInstance & ci, const ICAConfig & checks)
    : ICAVisitor(ci, checks)
{
    if (!isEnabled()) {
        return;
    }

    m_rand_id = getCustomDiagID(bad_rand, "'std::rand' is not thread-safe function. Consider using random generators (e.g., std::mt19937)");
    m_random_shuffle_id = getCustomDiagID(bad_rand, "std::random_shuffle was deprecated since C++14 and removed since C++17");
    m_engine_construction_id = getCustomDiagID(bad_rand, "same seed for a deterministic random engine produces same output");
    m_one_time_usage_id = getCustomDiagID(bad_rand, "constructing random engine for only a few numbers is not the best practice");
    m_use_distribution_id = getCustomDiagID(bad_rand, "you probably want to use 'uniform_int_distribution' here");
    m_slow_random_device_id = getCustomDiagID(bad_rand, "'std::random_device' may work too slow, please, only use it to init seed of some random engine");

    m_comp_stmt_stack.push_back(nullptr);
}

namespace {
bool is_random_engine_type(const clang::QualType & type)
{
    auto type_name = removeClassPrefix(type.getCanonicalType().getAsString());
    auto without_template_params = llvm::StringRef(type_name);
    without_template_params = without_template_params.substr(0, without_template_params.find('<'));
    return without_template_params.startswith("std::") && without_template_params.endswith("engine");
}

bool is_random_distribution_type(const clang::QualType & type)
{
    auto type_name = removeClassPrefix(type.getCanonicalType().getAsString());
    auto without_template_params = llvm::StringRef(type_name);
    without_template_params = without_template_params.substr(0, without_template_params.find('<'));
    return without_template_params.startswith("std::") && without_template_params.endswith("distribution");
}

bool is_defining_seed(const clang::Stmt * stmt)
{
    if (auto constr_expr = clang::dyn_cast<clang::CXXConstructExpr>(stmt)) {
        return is_random_engine_type(constr_expr->getType());
    } else if (auto method_call = clang::dyn_cast<clang::CXXMemberCallExpr>(stmt)) {
        if (auto method_decl = method_call->getMethodDecl()) {
            if (auto identifier = method_decl->getIdentifier(); identifier && identifier->isStr("seed")) {
                return is_random_engine_type(method_call->getImplicitObjectArgument()->getType());
            }
        }
    }
    return false;
}

}

bool CCBadRandVisitor::VisitCallExpr(clang::CallExpr * call)
{
    if (!shouldProcessExpr(call, getSM())) {
        return true;
    }

    if (auto func_decl = call->getDirectCallee()) {
        auto callee_identifier = func_decl->getIdentifier();
        if (!callee_identifier) {
            return true;
        }
        if (callee_identifier->isStr("rand")) {
            report(call->getExprLoc(), m_rand_id);
        } else if (callee_identifier->isStr("random_shuffle")) {
            report(call->getExprLoc(), m_random_shuffle_id);
        } else if (callee_identifier->isStr("move")) {
            auto engn_decl_ref = clang::dyn_cast<clang::DeclRefExpr>(call->getArg(0)->IgnoreParenCasts());
            if (engn_decl_ref) {
                m_rand_engns.get<EngnDecl>().erase(clang::dyn_cast<clang::VarDecl>(engn_decl_ref->getDecl()));
            }
        }
    }
    return true;
}

bool CCBadRandVisitor::VisitCXXConstructExpr(clang::CXXConstructExpr *constr)
{
    if (!shouldProcessExpr(constr, getSM())) {
        return true;
    }

    auto constructed_type = constr->getType();
    if (is_random_engine_type(constructed_type)) {
        if (constr->getNumArgs() < 1 ||
            constr->getArg(0)->isDefaultArgument() ||
            clang::isa<clang::IntegerLiteral>(constr->getArg(0)->IgnoreImpCasts())) {
            report(constr->getExprLoc(), m_engine_construction_id);
        }
        m_is_defining_seed = true;
    }
    return true;
}

bool CCBadRandVisitor::VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr *op_call)
{
    if (!shouldProcessExpr(op_call, getSM())) {
        return true;
    }

    if (op_call->getOperator() != clang::OO_Call) {
        return true;
    }

    auto method_decl = clang::dyn_cast<clang::CXXMethodDecl>(op_call->getDirectCallee());
    if (!method_decl) {
        return true;
    }

    const clang::Expr * engn_arg = nullptr;
    bool distribution_needed = false;
    const auto * record_decl = method_decl->getParent();
    if (op_call->getNumArgs() > 1) {
        if (is_random_distribution_type(op_call->getArg(0)->getType())) {
            engn_arg = op_call->getArg(1);
            if (!m_is_defining_seed && engn_arg->getType()->getAsTagDecl()->getName().equals("random_device")) {
                report(op_call->getExprLoc(), m_slow_random_device_id);
            }
        }
    } else if (op_call->getNumArgs() == 1) {
        if (is_random_engine_type(op_call->getArg(0)->getType())) { // check for call to 'operator ()' of random engine
            engn_arg = op_call->getArg(0);
            distribution_needed = true;
        } else if (!m_is_defining_seed && record_decl->getName() == llvm::StringRef("random_device")) {
            report(op_call->getExprLoc(), m_slow_random_device_id);
        }
    }

    if (!engn_arg) {
        return true;
    }

    engn_arg = engn_arg->IgnoreParenCasts();
    if (auto engn_decl_ref = clang::dyn_cast<clang::DeclRefExpr>(engn_arg); engn_decl_ref && m_cycle_depth > 0) {
        m_rand_engns.get<EngnDecl>().erase(clang::dyn_cast<clang::VarDecl>(engn_decl_ref->getDecl()));
    } else if (clang::isa<clang::CXXTemporaryObjectExpr>(engn_arg) || clang::isa<clang::CXXConstructExpr>(engn_arg)){
        distribution_needed = false; // cannot use distribution with rvalue anyway
        report(op_call->getExprLoc(), m_one_time_usage_id);
    }

    if (distribution_needed) {
        report(op_call->getExprLoc(), m_use_distribution_id);
    }


    return true;
}

bool CCBadRandVisitor::VisitVarDecl(clang::VarDecl *var_decl)
{
    if (!shouldProcessDecl(var_decl, getSM()) || !m_ctor_body_stack.empty()) { // don't report one-shot usage of engine if it's declared in ctor
        return true;
    }

    if (is_random_engine_type(var_decl->getType())) {
        m_rand_engns.insert({var_decl, m_comp_stmt_stack.back()});
    }
    return true;
}

bool CCBadRandVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *method_call)
{
    if (!shouldProcessExpr(method_call, getSM())) {
        return true;
    }

    const auto * method_decl = method_call->getMethodDecl();
    const auto caller_type = method_call->getImplicitObjectArgument()->getType();
    if (!method_decl) {
        return true;
    }
    auto identifier = method_decl->getIdentifier();
    m_is_defining_seed = identifier && identifier->isStr("seed") && is_random_engine_type(caller_type);
    return true;
}

bool CCBadRandVisitor::VisitCXXConstructorDecl(clang::CXXConstructorDecl * ctor_decl)
{
    if (!shouldProcessDecl(ctor_decl, getSM()) || !ctor_decl->doesThisDeclarationHaveABody()) {
        return true;
    }
    m_ctor_body_stack.push_back(ctor_decl->getBody());
    return true;
}

bool CCBadRandVisitor::dataTraverseStmtPre(clang::Stmt *stmt)
{
    if (clang::isa<clang::ForStmt>(stmt) || clang::isa<clang::CXXForRangeStmt>(stmt) || clang::isa<clang::WhileStmt>(stmt)) {
        ++m_cycle_depth;
    } else if (auto comp_stmt = clang::dyn_cast<clang::CompoundStmt>(stmt)) {
        m_comp_stmt_stack.push_back(comp_stmt);
    }
    return true;
}

bool CCBadRandVisitor::dataTraverseStmtPost(clang::Stmt *stmt)
{
    if (!stmt) {
        return true;
    }
    if (!m_ctor_body_stack.empty() && (stmt == m_ctor_body_stack.back())) {
        m_ctor_body_stack.pop_back();
    } else if (clang::isa<clang::ForStmt>(stmt) || clang::isa<clang::CXXForRangeStmt>(stmt) || clang::isa<clang::WhileStmt>(stmt)) {
        --m_cycle_depth;
    } else if (auto comp_stmt = clang::dyn_cast<clang::CompoundStmt>(stmt)) {
        auto [rep_begin, rep_end] = m_rand_engns.get<CompStmt>().equal_range(comp_stmt);
        std::for_each(rep_begin, rep_end, [this](const auto & to_report) {
            report(to_report.engn_decl->getLocation(), m_one_time_usage_id);
        });
        m_rand_engns.get<CompStmt>().erase(comp_stmt);
        m_comp_stmt_stack.pop_back();
    } else {
        if (is_defining_seed(stmt)) {
            m_is_defining_seed = false;
        }
    }
    return true;
}