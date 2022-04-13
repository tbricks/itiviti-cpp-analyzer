#include "shared/checks/EmplaceDefaultValueVisitor.h"

namespace ica {

EmplaceDefaultValueVisitor::EmplaceDefaultValueVisitor(clang::CompilerInstance & ci, const Config & config)
    : Visitor(ci, config)
{
    if (!isEnabled()) {
        return;
    }

    m_warn_id = getCustomDiagID(emplace_default_value, "use of '%0' with default value makes extra default constructor call");
}

bool EmplaceDefaultValueVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * expr)
{
    if (!shouldProcessExpr(expr, getSM())) {
        return true;
    }
    auto method_decl = expr->getMethodDecl();
    if (!method_decl) {
        return true;
    }

    std::string method_name = method_decl->getNameAsString();
    const bool is_emplace_or_try_emplace = method_name == "emplace" || method_name == "try_emplace";
    const bool is_two_args_emplace = is_emplace_or_try_emplace && expr->getNumArgs() == 2;
    const bool is_emplace_hint = method_name == "emplace_hint";
    const bool is_three_args_emplace_hint = is_emplace_hint && expr->getNumArgs();
    if ((is_two_args_emplace || is_three_args_emplace_hint) && hasTryEmplace(expr->getRecordDecl())) {
        auto value_arg = expr->getArg(expr->getNumArgs() - 1);

        if (auto mater_temp = clang::dyn_cast<clang::MaterializeTemporaryExpr>(value_arg); mater_temp) {
            clang::CXXTemporaryObjectExpr * temp_obj;
            if (auto bind_temp = clang::dyn_cast<clang::CXXBindTemporaryExpr>(mater_temp->getSubExpr()); bind_temp) {
                temp_obj = clang::dyn_cast<clang::CXXTemporaryObjectExpr>(bind_temp->getSubExpr());
            } else {
                temp_obj = clang::dyn_cast<clang::CXXTemporaryObjectExpr>(mater_temp->getSubExpr());
            }
            if (temp_obj && temp_obj->getConstructor()->isDefaultConstructor()) {
                makeReport(expr);
            }
        }
    }

    return true;
}

void EmplaceDefaultValueVisitor::clear()
{
    m_has_try_emplace.clear();
}

bool EmplaceDefaultValueVisitor::hasTryEmplace(const clang::CXXRecordDecl * decl)
{
    if (!decl) {
        return false;
    }

    auto [it, emplaced] = m_has_try_emplace.try_emplace(decl);
    if (emplaced) {
        const auto is_try_emplace = [](clang::Decl * sub_decl) {
            if (auto func_template = clang::dyn_cast<clang::FunctionTemplateDecl>(sub_decl)) {
                return func_template->getNameAsString() == "try_emplace";
            } else if (auto method_decl = clang::dyn_cast<clang::CXXMethodDecl>(sub_decl)) {
                return method_decl->getNameAsString() == "try_emplace";
            }
            return false;
        };
        it->second = std::any_of(decl->decls_begin(), decl->decls_end(), is_try_emplace);
    }

    return it->second;
}
void EmplaceDefaultValueVisitor::makeReport(const clang::CXXMemberCallExpr * method_call)
{
    auto method_name = method_call->getMethodDecl()->getNameAsString();
    auto expr_loc = method_call->getExprLoc();

    auto key_arg = method_call->getArg(method_call->getNumArgs() - 2);
    auto value_arg = method_call->getArg(method_call->getNumArgs() - 1);

    auto add_try = clang::FixItHint::CreateInsertion(expr_loc, "try_");

    bool is_ok = true;
    const char * key_string = getSM().getCharacterData(key_arg->getExprLoc(), &is_ok);
    const char * comma_char = key_string;
    if (is_ok) {
        while (*comma_char != ',') {
            ++comma_char;
        }
    }
    auto last_arg_range = clang::CharSourceRange::getCharRange(clang::SourceRange(key_arg->getExprLoc().getLocWithOffset(comma_char - key_string),
                                                                                  value_arg->getEndLoc().getLocWithOffset(1)));
    if (method_name == "emplace") {
        report(expr_loc, m_warn_id)
                .AddValue(method_name)
                .AddFixItHint(clang::FixItHint::CreateRemoval(last_arg_range))
                .AddFixItHint(add_try);
    } else if (method_name == "try_emplace"){
        report(expr_loc, m_warn_id)
                .AddValue(method_name)
                .AddFixItHint(clang::FixItHint::CreateRemoval(last_arg_range));
    } else { // method_name = "emplace_hint"
        const std::size_t low_line_idx = 7; // idx of '_' in "emplace_hint"
        const std::size_t last_idx = 11; // last char idx in "emplace_hint"
        auto remove_hint_loc = clang::SourceRange(expr_loc.getLocWithOffset(low_line_idx), expr_loc.getLocWithOffset(last_idx));
        report(expr_loc, m_warn_id)
                .AddValue(method_name)
                .AddFixItHint(add_try)
                .AddFixItHint(clang::FixItHint::CreateRemoval(last_arg_range))
                .AddFixItHint(clang::FixItHint::CreateRemoval(remove_hint_loc));
    }
}

} // namespace ica
