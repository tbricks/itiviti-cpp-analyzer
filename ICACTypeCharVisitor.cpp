#include "ICACTypeCharVisitor.h"
#include "ICACommon.h"
#include "ICADiagnosticsBuilder.h"

#include <algorithm>
#include <cassert>
#include <string_view>
#include <initializer_list>

ICACTypeCharVisitor::ICACTypeCharVisitor(clang::CompilerInstance & ci, const ICAConfig & checks)
    : ICAVisitor(ci, checks)
{
    if (!isEnabled()) return;

    m_warn_id = getCustomDiagID(no_named_object,
            "%q0 called with '%1' argument which may be UB. Use static_cast to unsigned char");
}

namespace {

constexpr std::initializer_list<std::string_view> ctype_pred_names =
{
    "isalnum", "isalpha", "islower", "isupper",
    "isdigit", "isxdigit", "iscntrl", "isgraph",
    "isspace", "isblank", "isprint", "ispunct",
    "tolower", "toupper" // not actually a pred, but same logic applies here
};

bool isCTypePredName(const std::string_view name)
{
    const auto pred = [name = removePrefix(name, "std::").second] (const auto pred_name) {
        return pred_name == name;
    };
    return std::any_of(ctype_pred_names.begin(), ctype_pred_names.end(), pred);
}

auto getCalleeAsDeclRefExpr(const clang::CallExpr * call_expr)
{
    return call_expr->getCallee()
        ? clang::dyn_cast<clang::DeclRefExpr>(call_expr->getCallee()->IgnoreParenImpCasts())
        : nullptr;
}

auto getAsFunctionDecl(const clang::NamedDecl * named_decl)
{
    if (const auto * using_shadow = clang::dyn_cast<clang::UsingShadowDecl>(named_decl); using_shadow) {
        return clang::dyn_cast_or_null<const clang::FunctionDecl>(using_shadow->getTargetDecl());
    }
    return clang::dyn_cast_or_null<const clang::FunctionDecl>(named_decl);
}

// arg is int, return type is int
bool checkCTypePredSignature(const clang::FunctionDecl * func_decl)
{
    const auto return_type = func_decl->getReturnType();
    if (return_type.getAsString() != "int") {
        return false;
    }
    if (func_decl->getNumParams() != 1) {
        return false;
    }
    const auto * param = func_decl->parameters()[0];
    if (!param) {
        return false;
    }
    if (param->getType().getAsString() != "int") {
        return false;
    }
    return true;
}

const clang::FunctionDecl * isCTypePredCall(const clang::CallExpr * call_expr)
{
    if (const auto * decl_ref_expr = getCalleeAsDeclRefExpr(call_expr); decl_ref_expr) {
        if (const auto *  found_decl = decl_ref_expr->getFoundDecl(); found_decl) {
            if (isCTypePredName(found_decl->getQualifiedNameAsString())) {
                if (const auto * func_decl = getAsFunctionDecl(found_decl); func_decl) {
                    if (checkCTypePredSignature(func_decl)) {
                        return func_decl;
                    }
                }
            }
        }
    }
    return nullptr;
}

std::string wrapStringWith(
        const std::string_view str,
        const std::string_view prefix,
        const std::string_view suffix)
{
    std::string result;
    result.reserve(prefix.size() + str.size() + suffix.size() + 1);
    result += prefix;
    result += str;
    result += suffix;
    return result;
}

} // namespace anonymous

void ICACTypeCharVisitor::reportExpr(
        const clang::CallExpr * call_expr,
        const clang::FunctionDecl * func_decl,
        const std::string & arg_type)
{
    const auto * arg = call_expr->getArg(0);

    const auto loc     = arg->getExprLoc();
    const auto loc_end = call_expr->getEndLoc().getLocWithOffset(-1);

    const auto range = clang::SourceRange(loc, loc_end);
    const auto str = sourceRangeAsString(range, getSM());

    const auto rep = wrapStringWith(str, "static_cast<unsigned char>(", ")");

    const auto hint = clang::FixItHint::CreateReplacement(range, rep);

    report(call_expr->getBeginLoc(), m_warn_id)
        .AddValue(func_decl)
        .AddValue(arg_type)
        .AddFixItHint(hint);
}

bool ICACTypeCharVisitor::VisitCallExpr(clang::CallExpr * call_expr)
{
    if (shouldProcessExpr(call_expr, getSM())) {
        if (const auto * func_decl = isCTypePredCall(call_expr); func_decl) {

            const auto arg_type = call_expr->getArg(0)->IgnoreParenImpCasts()->getType().getAsString();
            if (arg_type == "char" || arg_type == "signed char") {
                reportExpr(call_expr, func_decl, arg_type);
            }
        }
    }
    return true;
}

void ICACTypeCharVisitor::clear()
{
    resetContext();
}
