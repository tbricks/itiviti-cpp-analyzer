#pragma once

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"

#include "shared/common/Common.h"
#include "shared/common/Checks.h"
#include "shared/common/Config.h"
#include "shared/common/DiagnosticsBuilder.h"

#include <algorithm>
#include <array>

namespace ica {

template <class ... Ts>
constexpr auto make_check_names(const Ts & ... ts)
{
    return std::array<std::string_view, sizeof...(Ts)>{ std::string_view(ts)... };
}


class VisitorBase
{
private:
    template <class CheckNames>
    static bool computeEnabled(const Checks & checks, const CheckNames & check_names) noexcept
    {
        const auto is_check_enabled = [&checks] (const auto & check_name) {
            return static_cast<bool>(checks[check_name]);
        };

        return std::any_of(std::begin(check_names), std::end(check_names),
                is_check_enabled);
    }

    std::string appendCheckName(std::string format_string, const std::string_view check_name) const
    {
        format_string += " [";
        if (m_config.get_use_url()) {
            format_string += wrapCheckNameWithURL(check_name);
        } else {
            format_string += check_name;
        }
        format_string += ']';

        return format_string;
    }

public:
    template <class CheckNames>
    explicit VisitorBase(clang::CompilerInstance & ci, const Config & config, const CheckNames & check_names)
        : m_diag(ci.getDiagnostics())
        , m_config(config)
        , m_enabled(computeEnabled(m_config.get_checks(), check_names))
    {
    }

    void setContext(clang::ASTContext & context)
    { this->m_context = &context; }

    void resetContext()
    { this->m_context = nullptr; }

    bool isEnabled() const
    { return m_enabled; }

protected:
    clang::ASTContext & getContext()
    { return *m_context; }

    const clang::ASTContext & getContext() const
    { return *m_context; }

    clang::SourceManager & getSM()
    { return m_context->getSourceManager(); }

    const clang::SourceManager & getSM() const
    { return m_context->getSourceManager(); }

protected:
    auto report(const DiagnosticID diag_id)
    { return ica::report(m_diag, diag_id); }

    auto report(const clang::SourceLocation loc, const DiagnosticID diag_id)
    { return ica::report(m_diag, loc, diag_id); }

    DiagnosticID getCustomDiagID(const std::string_view check_name, std::string format_string)
    {
        const auto & check = getCheck(check_name);
        if (!check) {
            return 0;
        }

        format_string = appendCheckName(std::move(format_string), check_name);
        return m_diag.getDiagnosticIDs()->getCustomDiagID(check, format_string);
    }

    DiagnosticID getCustomDiagID(clang::DiagnosticIDs::Level level, llvm::StringRef format_string)
    { return m_diag.getDiagnosticIDs()->getCustomDiagID(level, format_string); }

    Check getCheck(const std::string_view check) const
    { return m_config.get_checks()[check]; }

private:
    clang::DiagnosticsEngine & m_diag;
    const Config & m_config;
    clang::ASTContext * m_context = nullptr;
    bool m_enabled = false;
};


template <class VisitorImpl>
class Visitor
    : public VisitorBase
    , public clang::RecursiveASTVisitor<VisitorImpl>

{
public:
    explicit Visitor(clang::CompilerInstance & ci, const Config & config)
        : VisitorBase(ci, config, VisitorImpl::check_names)
    {
    }
};

} // namespace ica
