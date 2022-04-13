#pragma once

#include "shared/common/Visitor.h"

namespace ica {

class NoexceptVisitor : public Visitor<NoexceptVisitor>
{
    static constexpr auto * noexcept_check = "redundant-noexcept";

public:

    explicit NoexceptVisitor(clang::CompilerInstance & ci, const Config & config);

    static constexpr auto check_names = make_check_names(noexcept_check);

    bool VisitFunctionDecl(clang::FunctionDecl * decl);

    void printDiagnostic(clang::ASTContext &) { }

private:

    DiagnosticID m_warn_id = 0;
    DiagnosticID m_note_id = 0;
};

} // namespace ica
