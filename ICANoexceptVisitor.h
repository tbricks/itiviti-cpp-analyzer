#include "ICAVisitor.h"

class ICANoexceptVisitor : public ICAVisitor<ICANoexceptVisitor>
{
    static constexpr auto * noexcept_check = "redundant-noexcept";

public:

    explicit ICANoexceptVisitor(clang::CompilerInstance & ci, const ICAConfig & config);

    static constexpr auto check_names = make_check_names(noexcept_check);

    bool VisitFunctionDecl(clang::FunctionDecl * decl);

    void printDiagnostic(clang::ASTContext &) { }

private:

    DiagnosticID m_warn_id = 0;
    DiagnosticID m_note_id = 0;
};