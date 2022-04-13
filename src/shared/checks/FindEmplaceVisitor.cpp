#include "shared/checks/FindEmplaceVisitor.h"
#include "shared/common/Common.h"
#include "shared/common/DiagnosticsBuilder.h"

#include "clang-c/Index.h"
#include "clang/AST/ASTFwd.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Type.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/SourceLocation.h"

#include "llvm/ADT/StringRef.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <iterator>

namespace ica {

FindEmplaceVisitor::FindEmplaceVisitor(clang::CompilerInstance & ci, const Config & config)
    : Visitor(ci, config)
    , m_mem_func_get_key_type(GetKeyTypeFunctor())
{
    if (!isEnabled()) return;

    m_find_emplace_warn_id = getCustomDiagID(find_emplace,
            "'%0' called after find(). Consider using 'const auto [%3, inserted] = %1.try_emplace(%2, ...)'");

    m_try_emplace_warn_id = getCustomDiagID(try_emplace,
            "'try_emplace' could be used instead of '%0'. Consider using %1.try_emplace()");

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

/// Returns true, if declaration has a copy constructor (and it is not deleted), false otherwise
bool hasMoveOrCopyConstructor(const clang::CXXRecordDecl * type_decl)
{
    if (!type_decl) {
        return false;
    }
    for (const auto & decl : type_decl->ctors()) {
        if (decl->isCopyOrMoveConstructor() && !decl->isDeleted()) {
            return true;
        }
    }
    return false;
}

} // namespace anonymous

bool FindEmplaceVisitor::isOperatorSquareBracketsInsideIf([[maybe_unused]] const clang::Expr * expr)
{
    return true;
}

const clang::VarDecl * FindEmplaceVisitor::getSameAsFindVar(const clang::CallExpr * call,
                                                               const clang::Expr * cont,
                                                               const clang::Expr * key)
{
    const bool ignore_side_effects = true;

    for (auto compound_stmt = m_curr_stmt;; compound_stmt = m_parent_of[compound_stmt]) {
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

/// Defines behaviour of function to be memorized
std::optional<clang::QualType> FindEmplaceVisitor::GetKeyTypeFunctor::operator()(const clang::CXXRecordDecl * decl) const
{
    const auto is_key_type_typedef = [](clang::Decl * sub_decl) {
            auto typedef_decl = clang::dyn_cast<clang::TypedefNameDecl>(sub_decl);
            return typedef_decl && (typedef_decl->getIdentifier()->isStr("key_type"));
        };
        const auto res = std::find_if(decl->decls_begin(), decl->decls_end(), is_key_type_typedef);
        if (res != decl->decls_end()) {
            const auto * typedef_decl = clang::dyn_cast<clang::TypedefNameDecl>(*res);
            if (typedef_decl) {
                return typedef_decl->getUnderlyingType().getCanonicalType();
            }
        }
    return std::nullopt;
}

/// Checks in lazy way (through MemorizingFunctor), whether the typedef/alias for "key_value" matches the type of argument
bool FindEmplaceVisitor::isKeyTypeCopyableOrMovable(const clang::CXXRecordDecl * decl)
{
    if (!decl) {
        return false;
    }
    const auto key_qual_type_optional = m_mem_func_get_key_type(decl);
    if (!key_qual_type_optional.has_value()) {
        return false;
    }
    const auto * rec_decl = key_qual_type_optional.value()->getAsCXXRecordDecl();
    if (rec_decl) {
        return hasMoveOrCopyConstructor(rec_decl);
    } else {
        return key_qual_type_optional.value()->isBuiltinType();
    }
}

bool FindEmplaceVisitor::VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * expr)
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

namespace {

bool isBadContainer(const clang::QualType & type)
{
    const auto raw_name = getUnqualifiedClassName(type);
    auto cut_template_name = llvm::StringRef(raw_name);
    return cut_template_name.startswith("std::unordered_map") || cut_template_name.startswith("std::map");
}

bool isNotPiecewiseConstructVariable(const clang::Expr * arg)
{
    if (!arg) {
        return true;
    }

    return getUnqualifiedClassName(arg->getType()) != "std::piecewise_construct_t";
}

} //namesapce anonymous

///  Returns true, if method call is good context for "try_emplace" insertion
///
/// \param func_name should match expr name (expr->getMethodDecl()->getNameInfo().getAsString())
///
/// \param type should match expr type (expr->getImplicitObjectArgument()->getType())
///
bool FindEmplaceVisitor::verifyTryEmplace(const clang::CXXMemberCallExpr * expr,
                                             const std::string_view func_name,
                                             const clang::QualType & type)
{
    unsigned arg_to_check = 0;
    if (func_name == "emplace_hint") {
        arg_to_check++;
    } else if (func_name != "emplace") {
        return false;
    }
    return isBadContainer(type) &&
           expr->getNumArgs() > arg_to_check &&
           isNotPiecewiseConstructVariable(expr->getArg(arg_to_check)) &&
           isKeyTypeCopyableOrMovable(expr->getRecordDecl());
}

bool FindEmplaceVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * expr)
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

    if (verifyTryEmplace(expr, func_name, obj_expr->getType())) {
        makeTryEmplaceReport(expr);
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

/// finds all local stringstream decls
bool FindEmplaceVisitor::VisitVarDecl(clang::VarDecl * var_decl)
{
    if (shouldProcessDecl(var_decl, getSM()) && var_decl->isLocalVarDecl()) {
        if (const auto [cont, key, find] = getContainerAndKey(var_decl->getInit()); cont && key && find) {
            m_stmt_iterators[m_curr_stmt].try_emplace(var_decl, cont, key, find);
        }
    }

    return true;
}

void FindEmplaceVisitor::clear()
{
    resetContext();
    m_calls.clear();
    m_stmt_iterators.clear();
    m_curr_stmt = nullptr;
}

bool FindEmplaceVisitor::dataTraverseStmtPre(clang::Stmt * stmt)
{
    if (auto new_compound_stmt = clang::dyn_cast_or_null<clang::CompoundStmt>(stmt)) {
        m_parent_of.emplace(new_compound_stmt, m_curr_stmt);
        m_stmt_iterators.try_emplace(new_compound_stmt);
        m_curr_stmt = new_compound_stmt;
    }

    return true;
}

bool FindEmplaceVisitor::dataTraverseStmtPost(clang::Stmt * stmt)
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

void FindEmplaceVisitor::makeTryEmplaceReport(const clang::CXXMemberCallExpr * method_call_expr)
{
    if (!getCheck(try_emplace))
        return;
    auto method_name = method_call_expr->getMethodDecl()->getName();
    auto method_loc = method_call_expr->getExprLoc();
    auto obj_expr_string = getContName(getSM(), method_call_expr);
    auto add_try = clang::FixItHint::CreateInsertion(method_loc, "try_");
    auto remove_hint_suffix = [&] (DiagnosticBuilder && diag_builder) {
        if (method_name.equals("emplace_hint")) {
            auto remove_hint_suffix = clang::FixItHint::CreateRemoval(clang::SourceRange(
                method_loc.getLocWithOffset(7),
                method_loc.getLocWithOffset(12)
            ));
            std::move(diag_builder).AddFixItHint(remove_hint_suffix);
        }
    };
    remove_hint_suffix(
        report(method_loc, m_try_emplace_warn_id)
            .AddValue(method_name)
            .AddValue(obj_expr_string)
            .AddFixItHint(add_try)
    );
}

void FindEmplaceVisitor::reportCompoundStmt()
{
    if (!getCheck(find_emplace))
        return;
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

        report(expr_loc, m_find_emplace_warn_id)
                .AddValue(callee)
                .AddValue(cont_name)
                .AddValue(key_as_string)
                .AddValue(iter_name);

        report_find(find_call);
    }
    m_calls.clear();
}

} // namespace ica
