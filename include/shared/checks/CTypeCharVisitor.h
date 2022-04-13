#pragma once

#include "shared/common/Visitor.h"

#include <string>
#include <utility>
#include <tuple>

namespace ica {

class CTypeCharVisitor : public Visitor<CTypeCharVisitor>
{
    static constexpr inline auto * no_named_object = "char-in-ctype-pred";

public:
    explicit CTypeCharVisitor(clang::CompilerInstance & ci, const Config & checks);

public:
    static constexpr inline auto check_names = make_check_names(no_named_object);

public:
    bool VisitCallExpr(clang::CallExpr * expr);

    void clear();
    void printDiagnostic(clang::ASTContext & context) { }

    bool shouldVisitTemplateInstantiations() const
    { return true; }

private:
    void reportExpr(
            const clang::CallExpr * call_expr,
            const clang::FunctionDecl * func_decl,
            const std::string & arg_type);

private:
    DiagnosticID m_warn_id = 0;
};

} // namespace ica
