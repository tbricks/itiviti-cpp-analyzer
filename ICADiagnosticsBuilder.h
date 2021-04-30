#pragma once

#include "clang/AST/Decl.h"
#include "clang/Basic/Diagnostic.h"

#include <cstddef>
#include <type_traits>
#include <utility>

using DiagnosticID = unsigned int;

class ICADiagnosticBuilder;

inline ICADiagnosticBuilder report(clang::DiagnosticsEngine &, DiagnosticID);
inline ICADiagnosticBuilder report(clang::DiagnosticsEngine &, clang::SourceLocation, DiagnosticID);

class ICADiagnosticBuilder
{
    friend ICADiagnosticBuilder report(clang::DiagnosticsEngine &, DiagnosticID);
    friend ICADiagnosticBuilder report(clang::DiagnosticsEngine &, clang::SourceLocation, DiagnosticID);

    ICADiagnosticBuilder(
            clang::DiagnosticsEngine & de,
            const clang::SourceLocation & loc,
            const DiagnosticID diag_id)
        : m_builder(de.Report(loc, diag_id))
    { }
    ICADiagnosticBuilder(
            clang::DiagnosticsEngine & de,
            const DiagnosticID diag_id)
        : m_builder(de.Report(diag_id))
    { }

public:
    ICADiagnosticBuilder(const ICADiagnosticBuilder &) = delete;
    ICADiagnosticBuilder & operator = (const ICADiagnosticBuilder &) = delete;

public:
    auto && AddValue(clang::StringRef s) &&
    {
        m_builder.AddString(std::move(s));
        return std::move(*this);
    }

    auto && AddValue(const clang::NamedDecl * named_decl) &&
    {
        m_builder.AddTaggedVal(reinterpret_cast<std::intptr_t>(named_decl), clang::DiagnosticsEngine::ArgumentKind::ak_nameddecl);
        return std::move(*this);
    }

    template <class T, class = std::enable_if_t<std::is_integral_v<T>>>
    auto && AddValue(const T value) &&
    {
        const auto arg_kind = std::is_signed_v<T>
            ? clang::DiagnosticsEngine::ArgumentKind::ak_sint
            : clang::DiagnosticsEngine::ArgumentKind::ak_uint;

        m_builder.AddTaggedVal(static_cast<std::intptr_t>(value), arg_kind);
        return std::move(*this);
    }

    auto && AddSourceRange(const clang::CharSourceRange range) &&
    {
        m_builder.AddSourceRange(range);
        return std::move(*this);
    }

    auto && AddSourceRange(const clang::SourceRange range) &&
    {
        return std::move(*this).AddSourceRange(clang::CharSourceRange::getCharRange(range));
    }

    auto && AddFixItHint(const clang::FixItHint & hint) &&
    {
        m_builder.AddFixItHint(hint);
        return std::move(*this);
    }

private:
    clang::DiagnosticBuilder m_builder;
};

inline ICADiagnosticBuilder report(
        clang::DiagnosticsEngine & de,
        const DiagnosticID diag_id)
{
    return {de, diag_id};
}

inline ICADiagnosticBuilder report(
        clang::DiagnosticsEngine & de,
        const clang::SourceLocation loc,
        const DiagnosticID diag_id)
{
    return {de, loc, diag_id};
}
