#pragma once

#include "ICAVisitor.h"

class ICARemoveCStrVisitor : public ICAVisitor<ICARemoveCStrVisitor>
{
    static constexpr inline auto * remove_c_str = "remove-c_str";

public:
    explicit ICARemoveCStrVisitor(clang::CompilerInstance & ci, const ICAConfig & config);

public:
    static constexpr inline auto check_names = make_check_names(remove_c_str);

public:
    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * expr);

public:
    void printDiagnostic(clang::ASTContext & context) { }

private:
    DiagnosticID m_replace_method_id = 0;
    DiagnosticID m_implicit_cast_id = 0;
    DiagnosticID m_note_id = 0;
};
