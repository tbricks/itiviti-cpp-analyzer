#include "ICAFindEmplaceVisitor.h"
#include "ICACommon.h"
#include "ICADiagnosticsBuilder.h"

#include <cassert>
#include <algorithm>
#include <unordered_set>
#include <iterator>

ICAFindEmplaceVisitor::ICAFindEmplaceVisitor(clang::CompilerInstance & ci, const ICAConfig & config)
    : ICAVisitor(ci, config)
{
    if (!isEnabled()) return;

    m_warn_id = getCustomDiagID(find_emplace,
            "'%0' called after find(). Consider using 'const auto [%3, inserted] = %1.emplace(%2, ...)'");

    m_note_id = getCustomDiagID(clang::DiagnosticIDs::Note,
            "'find' called here");

    m_parent_of.emplace(nullptr, nullptr);
    m_stmt_iterators.try_emplace(nullptr);
    m_curr_stmt = nullptr;
}

namespace {

const clang::Expr * passBindingExpr(const clang::Expr * expr)
{
    if (auto * bind_temp_expr = clang::dyn_cast<clang::CXXBindTemporaryExpr>(expr); bind_temp_expr) {
        if (const auto * sub_expr = clang::dyn_cast<clang::CXXConstructExpr>(bind_temp_expr->getSubExpr());
            sub_expr && sub_expr->getNumArgs() > 0) {
            if (sub_expr->getNumArgs() > 1 &&
                std::any_of(++sub_expr->child_begin(), sub_expr->child_end(), [](const clang::Stmt * arg)
                    { return clang::dyn_cast<clang::CXXDefaultArgExpr>(arg) == nullptr; })) {
                return sub_expr; // if expression isn't constructed with single argument, we shouldn't ignore rest of them
            }
            return sub_expr->getArg(0)->IgnoreParenCasts();
        }
        return bind_temp_expr->getSubExpr();
    }
    return expr;
}

bool isSameKeyExpr(
        clang::ASTContext & context,
        const clang::Expr * key1,
        const clang::Expr * key2,
        const bool ignore_side_effects)
{
    const auto * k1 = passBindingExpr(key1->IgnoreParenCasts());
    const auto * k2 = passBindingExpr(key2->IgnoreParenCasts());

    return isIdenticalStmt(context,
            k1,
            k2,
            ignore_side_effects);
}

bool isSameContExpr(
        clang::ASTContext & context,
        const clang::Expr * cont1,
        const clang::Expr * cont2,
        const bool ignore_side_effects)
{
    const auto * c1 = cont1->IgnoreParenImpCasts();
    const auto * c2 = cont2->IgnoreParenImpCasts();

    return isIdenticalStmt(context,
            c1,
            c2,
            ignore_side_effects);
}

const clang::Expr * getFirstArgOfPair(const clang::CXXConstructExpr * cxx_ctor_expr)
{
    if (cxx_ctor_expr) {
        if (cxx_ctor_expr->getNumArgs() > 1) {
            return cxx_ctor_expr->getArg(0);
        }
    }
    return nullptr;
}

const clang::Expr * getFirstArgOfPair(const clang::ExprWithCleanups * expr_with_cleanups)
{
    if (expr_with_cleanups) {
        const auto * sub_expr = expr_with_cleanups->getSubExpr()->IgnoreParenImpCasts();
        return getFirstArgOfPair(clang::dyn_cast<clang::CXXConstructExpr>(sub_expr));
    }
    return nullptr;
}

const clang::Expr * getFirstArgOfPair(const clang::Expr * arg_expr)
{
    if (const auto * mat_temp_expr = clang::dyn_cast<clang::MaterializeTemporaryExpr>(arg_expr); mat_temp_expr) {
        // if call expr like make_pair(id, ..)
        const auto * bind_sub_expr = passBindingExpr(mat_temp_expr->getSubExpr());
        if (const auto * call_expr = clang::dyn_cast<clang::CallExpr>(bind_sub_expr); call_expr) {
            if (call_expr->getNumArgs() == 2) {
                if (const auto * callee_decl = call_expr->getDirectCallee(); callee_decl) {
                    const auto name = callee_decl->getQualifiedNameAsString();
                    if (name == "std::make_pair") {
                        return call_expr->getArg(0);
                    }
                }
            }
            return nullptr;
        }
        return getFirstArgOfPair(clang::dyn_cast<clang::CXXConstructExpr>(bind_sub_expr));
    }

    if (const auto * decl_ref_expr = clang::dyn_cast<clang::DeclRefExpr>(arg_expr); decl_ref_expr) {
        if (const auto * var_decl = clang::dyn_cast<clang::VarDecl>(decl_ref_expr->getDecl()); var_decl) {
            if (var_decl->getInit()) {
                return getFirstArgOfPair(clang::dyn_cast<clang::ExprWithCleanups>(var_decl->getInit()));
            }
        }
    }

    return nullptr;
}

struct ContKeyFind
{
    const clang::Expr * cont = nullptr;
    const clang::Expr * key  = nullptr;
    const clang::Expr * find = nullptr;
};

ContKeyFind getContainerAndKey(const clang::CXXMemberCallExpr * mem_call_expr)
{
    if (!mem_call_expr) {
        return {};
    }

    if (const auto * method_decl = mem_call_expr->getMethodDecl(); method_decl) {
        const auto method_name = method_decl->getNameInfo().getAsString();

        if (method_name == "find" && mem_call_expr->getNumArgs() == 1) {

            const auto * arg = mem_call_expr->getArg(0);
            const auto * obj = mem_call_expr->getImplicitObjectArgument();

            return {obj, arg, mem_call_expr};
        }
    }

    return {};
}

ContKeyFind getContainerAndKey(const clang::ImplicitCastExpr * impl_cast_expr)
{
    if (!impl_cast_expr) {
        return {};
    }

    if (const auto * mem_call_expr = clang::dyn_cast<clang::CXXMemberCallExpr>(impl_cast_expr->getSubExpr()); mem_call_expr) {
        return getContainerAndKey(mem_call_expr);
    }

    return {};
}


ContKeyFind getContainerAndKey(const clang::Expr * init_expr)
{
    if (!init_expr) {
        return {};
    }

    if (const auto * expr_with_cleanups = clang::dyn_cast<clang::ExprWithCleanups>(init_expr); expr_with_cleanups) {
        return getContainerAndKey(expr_with_cleanups->getSubExpr());
    }

    if (const auto * impl_cast_expr = clang::dyn_cast<clang::ImplicitCastExpr>(init_expr); impl_cast_expr) {
        return getContainerAndKey(impl_cast_expr);
    }
    if (const auto * mem_call_expr = clang::dyn_cast<clang::CXXMemberCallExpr>(init_expr); mem_call_expr) {
        return getContainerAndKey(mem_call_expr);
    }

    return {};
}

} // namespace anonymous

bool ICAFindEmplaceVisitor::isOperatorSquareBracketsInsideIf([[maybe_unused]] const clang::Expr * expr)
{
    return true;
}

const clang::VarDecl * ICAFindEmplaceVisitor::getSameAsFindVar(const clang::CallExpr * call, const clang::Expr * cont, const clang::Expr * key)
{
    const bool ignore_side_effects = true;

    for (auto compound_stmt = m_curr_stmt;; compound_stmt = m_parent_of[compound_stmt]){
        for (const auto & [var, cont_key_find] : m_stmt_iterators[compound_stmt]) {
            const auto [it_cont, it_key, _] = cont_key_find;

            const bool same_key  = isSameKeyExpr (getContext(), key , it_key , ignore_side_effects);
            const bool same_cont = isSameContExpr(getContext(), cont, it_cont, ignore_side_effects);
            // const bool common_comp_stmt = isSameCompoundStmt(getContext(), call, it_find, ignore_side_effects);

            if (same_cont && same_key) {
                return var;
            }
        }
        if (compound_stmt == nullptr) {
            break;
        }
    }


    return nullptr;
}

bool ICAFindEmplaceVisitor::VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * expr)
{
    if (!shouldProcessExpr(expr, getSM())) {
        return true;
    }

    if (expr->getOperator() == clang::OO_Subscript) {
        if (isOperatorSquareBracketsInsideIf(expr)) {
            if (expr->getNumArgs() == 2) {
                if (auto * var_decl = getSameAsFindVar(expr, expr->getArg(0), expr->getArg(1)); var_decl) {
                    m_calls.emplace_back(var_decl, expr);
                }
            }
        }
    }

    return true;
}

bool ICAFindEmplaceVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * expr)
{
    if (!shouldProcessExpr(expr, getSM())) {
        return true;
    }

    auto * method_decl = expr->getMethodDecl(); // CHECK WHEN NULL
    if (!method_decl) {
        return true;
    }

    const auto func_name = method_decl->getNameInfo().getAsString();
    const auto obj_expr = expr->getImplicitObjectArgument();

    if (!obj_expr) {
        return true;
    }

    if (func_name == "emplace" && expr->getNumArgs() > 1) {
        if (auto * var_decl = getSameAsFindVar(expr, obj_expr, expr->getArg(0)); var_decl) {
            m_calls.emplace_back(var_decl, expr);
        }
    }

    if (func_name == "insert" && expr->getNumArgs() == 1) {
        if (const auto * first_pair_arg = getFirstArgOfPair(expr->getArg(0)); first_pair_arg) {
            if (const auto * var_decl = getSameAsFindVar(expr, obj_expr, first_pair_arg); var_decl) {
                m_calls.emplace_back(var_decl, expr);
            }
        }
    }

    return true;
}

// find all local stringstream decls
bool ICAFindEmplaceVisitor::VisitVarDecl(clang::VarDecl * var_decl)
{
    if (shouldProcessDecl(var_decl, getSM()) && var_decl->isLocalVarDecl()) {
        if (const auto [cont, key, find] = getContainerAndKey(var_decl->getInit()); cont && key && find) {
            m_stmt_iterators[m_curr_stmt].try_emplace(var_decl, cont, key, find);
        }
    }

    return true;
}

void ICAFindEmplaceVisitor::clear()
{
    resetContext();
    m_calls.clear();
    m_stmt_iterators.clear();
    m_curr_stmt = nullptr;
}

bool ICAFindEmplaceVisitor::dataTraverseStmtPre(clang::Stmt * stmt)
{
    if (auto new_compound_stmt = clang::dyn_cast_or_null<clang::CompoundStmt>(stmt)) {
        m_parent_of.emplace(new_compound_stmt, m_curr_stmt);
        m_stmt_iterators.try_emplace(new_compound_stmt);
        m_curr_stmt = new_compound_stmt;
    }

    return true;
}

bool ICAFindEmplaceVisitor::dataTraverseStmtPost(clang::Stmt * stmt)
{
    if (auto comp_stmt = clang::dyn_cast<clang::CompoundStmt>(stmt)) {
        reportCompoundStmt();
        m_curr_stmt = m_parent_of[m_curr_stmt];
        m_stmt_iterators.erase(comp_stmt);
    }
    return true;
}

namespace {

std::string getContName(const clang::SourceManager & sm, const clang::CXXMemberCallExpr * call_expr)
{
    const auto range = clang::SourceRange{call_expr->getBeginLoc(), call_expr->getExprLoc()};
    auto cont_name = static_cast<std::string>(sourceRangeAsString(range, sm));

    // remove 2 last chars
    if (!cont_name.empty()) cont_name.pop_back();
    if (!cont_name.empty()) cont_name.pop_back();

    return cont_name;
}

std::string getContName(const clang::SourceManager & sm, const clang::CXXOperatorCallExpr * call_expr)
{
    const auto range = clang::SourceRange{call_expr->getBeginLoc(), call_expr->getArg(1)->getBeginLoc()};
    auto cont_name = static_cast<std::string>(sourceRangeAsString(range, sm));

    // remove 2 last chars
    if (!cont_name.empty()) cont_name.pop_back();
    if (!cont_name.empty()) cont_name.pop_back();

    return cont_name;
}

} // namespace anonymous

void ICAFindEmplaceVisitor::reportCompoundStmt()
{
    auto iter_of_decl = [this](const clang::VarDecl * decl) {
        for (auto comp_stmt = m_curr_stmt;; comp_stmt = m_parent_of[comp_stmt]) {
            auto iterators_it = m_stmt_iterators.find(comp_stmt);
            if (iterators_it != m_stmt_iterators.end()) {
                auto container_key_find_it = iterators_it->second.find(decl);
                if (container_key_find_it != iterators_it->second.end()) {
                    return container_key_find_it->second;
                }
            }
            if (!comp_stmt) {
                break;
            }
        }
        return ContainerKeyFind();
    };

    auto report_find = [this] (const clang::Expr * find_call) {
        if (!find_call) {
            return;
        }

        report(find_call->getExprLoc(), m_note_id);
    };

    std::string cont_name;
    clang::SourceLocation expr_loc;
    std::string callee;

    for (auto call : m_calls) {
        auto var_decl = call.first;
        auto call_expr = call.second;
        auto container_key_find = iter_of_decl(var_decl);

        const auto * key_expr = std::get<1>(container_key_find);
        const auto * find_call = std::get<2>(container_key_find);
        clang::SourceRange sr(key_expr->getBeginLoc(), find_call->getEndLoc());

        auto key_as_string = std::string{sourceRangeAsString(sr, getSM())};
        if (!key_as_string.empty()) key_as_string.pop_back();

        std::string iter_name(var_decl->getName());

        if (auto oper_call = clang::dyn_cast_or_null<clang::CXXOperatorCallExpr>(call_expr)) {
            expr_loc = oper_call->getOperatorLoc();
            cont_name = getContName(getSM(), oper_call);
            callee = "operator[]";
        } else if (auto member_call = clang::dyn_cast_or_null<clang::CXXMemberCallExpr>(call_expr)) {
            expr_loc = member_call->getExprLoc();
            cont_name = getContName(getSM(), member_call);
            callee = member_call->getMethodDecl()->getNameInfo().getAsString();
        }

        report(expr_loc, m_warn_id)
                .AddValue(callee)
                .AddValue(cont_name)
                .AddValue(key_as_string)
                .AddValue(iter_name);

        report_find(find_call);
    }
    m_calls.clear();
}