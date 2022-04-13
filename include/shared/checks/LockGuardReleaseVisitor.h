#pragma once

#include "shared/common/Visitor.h"

#include <vector>

namespace ica {

class LockGuardReleaseVisitor : public Visitor<LockGuardReleaseVisitor>
{
    static constexpr inline auto * release_lock = "release-lock";

public:
    explicit LockGuardReleaseVisitor(clang::CompilerInstance & ci, const Config & config);

public:
    static constexpr inline auto check_names = make_check_names(release_lock);

public:
    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * ce);
    bool shouldVisitTemplateInstantiations () const
    { return true; }

    void printDiagnostic(clang::ASTContext & context);

private:
    std::map<const clang::VarDecl *, std::string> m_potential_mutex_not_unlocked;
    std::vector<std::pair<clang::CXXMemberCallExpr *, std::string>> m_release_unique_shared_lock;

    DiagnosticID m_warn_release_result_unused_id = 0;
    DiagnosticID m_warn_mutex_not_unlocked_id = 0;
    DiagnosticID m_warn_bad_code = 0;
};

} // namespace ica
