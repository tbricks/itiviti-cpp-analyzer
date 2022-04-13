#pragma once

#include "shared/common/Config.h"
#include "shared/common/DiagnosticsBuilder.h"
#include "shared/common/Visitor.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Frontend/CompilerInstance.h"

namespace ica {

class InlineMethodsInClassBodyVisitor : public Visitor<InlineMethodsInClassBodyVisitor>
{
    static constexpr auto * inline_methods_in_class = "inline-methods-in-class";

public:
    static constexpr auto check_names = make_check_names(inline_methods_in_class);

private:
    void makeInlineRemovalReport(const clang::CXXMethodDecl * decl);
    void makeInlineInsertionReport(const clang::FunctionDecl * decl);
public:
    InlineMethodsInClassBodyVisitor(
        clang::CompilerInstance & ci,
        const Config & config);

    bool VisitCXXRecordDecl(clang::CXXRecordDecl * decl);

    void printDiagnostic(clang::ASTContext &) {}
    void clear() {}

private:
    DiagnosticID m_remove_inline_warn_id = 0;
    DiagnosticID m_insert_inline_note_id = 0;
};

} // namespace ica
