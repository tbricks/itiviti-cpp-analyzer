#include "shared/checks/MoveStringStreamVisitor.h"

#include <algorithm>
#include <string_view>

using namespace std::string_view_literals;

namespace ica {

MoveStringStreamVisitor::MoveStringStreamVisitor(clang::CompilerInstance & ci, const Config & config)
    : Visitor(ci, config)
{
    if (!isEnabled()) return;

    m_warn_id = getCustomDiagID(move_string_stream,
            "add std::move() to last std::stringstream usage to avoid allocation in C++20");
}

// TODO check args num of str
// find all str calls on stringstreams
bool MoveStringStreamVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * expr)
{
    if (!shouldProcessExpr(expr, getSM())) {
        return true;
    }

    clang::CXXMethodDecl * method_decl = expr->getMethodDecl();
    clang::Expr * impl_arg = expr->getImplicitObjectArgument();
    auto impl_cast = clang::dyn_cast<clang::ImplicitCastExpr>(impl_arg);
    if (impl_cast) {
        auto decl_ref = clang::dyn_cast<clang::DeclRefExpr>(impl_cast->getSubExpr());
        if (decl_ref) {
            auto decl = decl_ref->getDecl();
            if (m_all_references_to_string_streams.find(decl) != m_all_references_to_string_streams.end()
                    && method_decl->getName() == "str"sv) {
                m_decl_refs_to_string_streams_on_str_calls.emplace(decl, ExprList{decl_ref});
            }
        }
    }

    return true;
}

// find all references to stringstreams
bool MoveStringStreamVisitor::VisitDeclRefExpr(clang::DeclRefExpr * expr)
{
    if (!shouldProcessExpr(expr, getSM())) {
        return true;
    }

    auto decl = expr->getDecl();
    // is it call on local string stream ?
    const auto it = m_all_references_to_string_streams.find(decl);
    if (it != m_all_references_to_string_streams.end()) {
        it->second.push_back(expr);
    }

    return true;
}

// find all local stringstream decls
bool MoveStringStreamVisitor::VisitVarDecl(clang::VarDecl * decl)
{
    if (!shouldProcessDecl(decl, getSM())) {
        return true;
    }

    const std::vector<std::string> types = { "std::stringstream", "std::ostringstream" };

    if (decl->isLocalVarDecl()) {
        const auto it = std::find(types.begin(), types.end(), decl->getType().getAsString());
        if (it != types.end()) {
            m_all_references_to_string_streams.try_emplace(decl);
        }
    }

    return true;
}

void MoveStringStreamVisitor::clear()
{
    resetContext();
    m_all_references_to_string_streams.clear();
    m_decl_refs_to_string_streams_on_str_calls.clear();
}

void MoveStringStreamVisitor::printDiagnostic(clang::ASTContext & context)
{
    for (const auto & [ss_decl, usages] : m_all_references_to_string_streams) {
        const auto str_calls = m_decl_refs_to_string_streams_on_str_calls.find(ss_decl);
        if (str_calls != m_decl_refs_to_string_streams_on_str_calls.end()) {
            if (str_calls->second.empty()) {
                continue;
            } else {
                const auto last_str_call = str_calls->second.back();
                const auto last_ref_to_ss = usages.back();
                if (last_str_call == last_ref_to_ss) {
                    const auto loc = last_ref_to_ss->getBeginLoc();
                    const auto loc_end = last_ref_to_ss->getEndLoc();

                    const auto range = clang::SourceRange(loc, loc_end);
                    const std::string ss_name = clang::dyn_cast<clang::NamedDecl>(ss_decl)->getName();
                    const std::string replacement = "std::move(" + ss_name + ')';

                    auto hint = clang::FixItHint::CreateReplacement(range, replacement);

                    report(loc, m_warn_id)
                        .AddFixItHint(hint);
                }
            }
        }
    }
}

} // namespace ica
