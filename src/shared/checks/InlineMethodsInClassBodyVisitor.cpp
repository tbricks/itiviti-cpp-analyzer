#include "shared/checks/InlineMethodsInClassBodyVisitor.h"
#include "shared/common/Config.h"
#include "shared/common/Common.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/CompilerInstance.h"

namespace ica {

InlineMethodsInClassBodyVisitor::InlineMethodsInClassBodyVisitor(
                                                                    clang::CompilerInstance & ci,
                                                                    const Config & config)
    : Visitor(ci, config)
{
    if (!isEnabled()) {
        return;
    }

    m_remove_inline_warn_id =
        getCustomDiagID(inline_methods_in_class, "inline is used by default for method declarations/definitions in class body");
    m_insert_inline_note_id =
        getCustomDiagID(clang::DiagnosticIDs::Note, "Definition should be marked as inline instead of declaration");
}

namespace {
const auto * asMethodDecl(const clang::Decl * decl) {
    if (const auto * template_decl = clang::dyn_cast<clang::FunctionTemplateDecl>(decl)) {
        return clang::dyn_cast<clang::CXXMethodDecl>(template_decl->getTemplatedDecl());
    }
    return clang::dyn_cast<clang::CXXMethodDecl>(decl);
};
} // namespace anonymous

bool InlineMethodsInClassBodyVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl *decl)
{
    if (!shouldProcessDecl(decl, getSM())) {
        return true;
    }

    const auto check_method_decl = [this](const auto * method_decl) {
        if (method_decl && method_decl->isInlineSpecified() && !method_decl->isImplicit()) {
                makeInlineRemovalReport(method_decl);
        }
    };

    for (const auto * inner_decl : decl->decls()) {
        check_method_decl(asMethodDecl(inner_decl));
    }
    return true;
}

void InlineMethodsInClassBodyVisitor::makeInlineRemovalReport(const clang::CXXMethodDecl * method_decl) {
    const auto expr_loc_begin = method_decl->getBeginLoc();
    const auto inline_offset = sourceRangeAsString(method_decl->getSourceRange(), getSM())
                                    .find("inline");
    report(expr_loc_begin, m_remove_inline_warn_id)
        .AddFixItHint(clang::FixItHint::CreateRemoval(
             clang::SourceRange(
                    expr_loc_begin.getLocWithOffset(inline_offset),
                    expr_loc_begin.getLocWithOffset(inline_offset + 5)
                )
            )
        );
    if (const auto * function_definition = method_decl->getDefinition();
                     function_definition &&
                     !function_definition->isImplicit() &&
                     !function_definition->isInlineSpecified()) {
        makeInlineInsertionReport(function_definition);
    }
}

void InlineMethodsInClassBodyVisitor::makeInlineInsertionReport(const clang::FunctionDecl * function_decl)
{
    const auto expr_loc_begin = function_decl->getBeginLoc();
    report(expr_loc_begin, m_insert_inline_note_id)
        .AddFixItHint(clang::FixItHint::CreateInsertion(expr_loc_begin, "inline"));
}

} // namespace ica
