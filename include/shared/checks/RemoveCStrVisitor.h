#pragma once

#include "shared/common/Visitor.h"

namespace ica {

class RemoveCStrVisitor : public Visitor<RemoveCStrVisitor>
{
    static constexpr inline auto * remove_c_str = "remove-c_str";

public:
    explicit RemoveCStrVisitor(clang::CompilerInstance & ci, const Config & config);

public:
    static constexpr inline auto check_names = make_check_names(remove_c_str);

public:
    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * expr);
    bool VisitFunctionDecl(clang::FunctionDecl * function_decl);

public:
    void printDiagnostic(clang::ASTContext & context) { }

private:
    clang::FunctionDecl * m_last_visited_function_decl = nullptr;

private:
    DiagnosticID m_replace_method_id = 0;
    DiagnosticID m_implicit_cast_id = 0;
    DiagnosticID m_note_id = 0;
};

} // namespace ica
