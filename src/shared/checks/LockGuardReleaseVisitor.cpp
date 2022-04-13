#include "shared/checks/LockGuardReleaseVisitor.h"
#include "shared/common/Common.h"

namespace ica {

LockGuardReleaseVisitor::LockGuardReleaseVisitor(clang::CompilerInstance & ci, const Config & config)
    : Visitor(ci, config)
{
    if (!isEnabled()) return;

    m_warn_release_result_unused_id = getCustomDiagID(release_lock, "%0 'release()' method result unused, mutex is locked");
    m_warn_mutex_not_unlocked_id = getCustomDiagID(release_lock, "released mutex is not unlocked");
    m_warn_bad_code = getCustomDiagID(release_lock, "use %0.unlock() instead");
}

bool LockGuardReleaseVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * ce)
{
    if (!shouldProcessExpr(ce, getSM())) {
        return true;
    }

    auto method_decl = ce->getMethodDecl();
    if (!method_decl) {
        return true;
    }
    if (!method_decl->getIdentifier()) { // avoid assert for method_decl->getName()
        return true;
    }

    // if result of `release` was assigned to some variable then looking for `unlock` was used directly on mutex
    if (const auto str_method = method_decl->getName(); !m_potential_mutex_not_unlocked.empty() && str_method == llvm::StringRef("unlock")) {
        if (auto implicit_arg = ce->getImplicitObjectArgument(); implicit_arg) {
            if (auto implicit_cast_expr = clang::dyn_cast<clang::ImplicitCastExpr>(implicit_arg); implicit_cast_expr) {
                if (auto decl_ref_expr = clang::dyn_cast<clang::DeclRefExpr>(implicit_cast_expr->getSubExpr()); decl_ref_expr) {
                    if (auto var_decl = clang::dyn_cast<clang::VarDecl>(decl_ref_expr->getDecl()); var_decl) {
                        m_potential_mutex_not_unlocked.erase(var_decl);
                    }
                }
            }
        }
    } else if (str_method != llvm::StringRef("release")) {
        return true;
    }

    auto record_decl = ce->getRecordDecl();
    if (!record_decl ||
            !(record_decl = record_decl->getCanonicalDecl()) ||
            (record_decl->getQualifiedNameAsString() != "std::unique_lock" &&
            record_decl->getQualifiedNameAsString() != "std::shared_lock")) {
        return true;
    }

    const auto parents = getContext().getParents(*ce);
    bool should_warn = true;
    for (const auto & parent : parents) { // Iterating by all parents of current call of ::release method (not sure there really can be > 1 parents)
        should_warn = true; // in case several parents of template (not sure it is possible in reality)
        if (const auto var = parent.get<clang::VarDecl>(); var) { // auto * mutex = lock.release();
            m_potential_mutex_not_unlocked.emplace(var, record_decl->getQualifiedNameAsString());
            should_warn = false;
        } else if (const auto bin_op = parent.get<clang::BinaryOperator>(); bin_op) { // std::mutex * p; p = m.release();
            if (const bool assign = bin_op->getOpcode() == clang::BinaryOperatorKind::BO_Assign; assign) {
                if (const auto lhs = bin_op->getLHS(); lhs) { // Investigating that `unlock` was called for this variable
                    if (auto decl_ref = clang::dyn_cast<clang::DeclRefExpr>(lhs); decl_ref) { // always decl ref in between
                        if (auto var_decl = clang::dyn_cast<clang::VarDecl>(decl_ref->getDecl()); var_decl) { // variable declaration
                            // saving this variable and will check that it is unlocked somewhere
                            m_potential_mutex_not_unlocked.emplace(var_decl, record_decl->getQualifiedNameAsString());
                            should_warn = false;
                        }
                    }
                }
            }
        } else if (const auto member_exp = parent.get<clang::MemberExpr>(); member_exp) {
            if (auto member_decl = member_exp->getMemberDecl(); member_decl && (member_decl->getIdentifier() && member_decl->getName() == llvm::StringRef("unlock"))) { // m.release()->unlock();
                std::string var_name;
                auto location = member_decl->getLocation();
                if (auto this_arg = ce->getImplicitObjectArgument(); this_arg) { // getting variable name
                    if (auto ref = llvm::dyn_cast<clang::DeclRefExpr>(this_arg); ref) {
                        location = ref->getLocation();
                        if (auto decl = ref->getDecl(); decl) {
                            var_name = decl->getNameAsString();
                        }
                    }
                }
                report(location, m_warn_bad_code).AddValue((var_name.empty() ? "lock" : var_name));
                should_warn = false;
            }
        }
    }
    if (!should_warn) {
        return true;
    }
    m_release_unique_shared_lock.emplace_back(ce, record_decl->getQualifiedNameAsString());

    return true;
}

void LockGuardReleaseVisitor::printDiagnostic(clang::ASTContext & context)
{
    for (auto & [call_expr, class_name] : m_release_unique_shared_lock) {
        report(call_expr->getExprLoc(), m_warn_release_result_unused_id).AddValue(class_name);
    }
    for (auto & [not_unlocked, class_name] : m_potential_mutex_not_unlocked) {
        report(not_unlocked->getBeginLoc(), m_warn_mutex_not_unlocked_id).AddValue(class_name);
    }
}

} // namespace ica
