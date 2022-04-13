#pragma once

#include "shared/common/Visitor.h"

#include <unordered_map>

namespace ica {

class EmplaceDefaultValueVisitor : public Visitor<EmplaceDefaultValueVisitor>
{
    static constexpr inline auto * emplace_default_value = "emplace-default-value";

public:
    explicit EmplaceDefaultValueVisitor(clang::CompilerInstance & ci, const Config & config);

    static constexpr inline auto check_names = make_check_names(emplace_default_value);

    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * expr);

    void printDiagnostic(clang::ASTContext & context) { }

    void clear();

private:
    bool hasTryEmplace(const clang::CXXRecordDecl * decl);

    void makeReport(const clang::CXXMemberCallExpr * method_call);

    DiagnosticID m_warn_id = 0;
    std::unordered_map<const clang::CXXRecordDecl *, bool> m_has_try_emplace;
};

} // namespace ica
