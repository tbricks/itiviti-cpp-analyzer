#include "internal/checks/InitMembersVisitor.h"

#include "shared/common/Common.h"
#include "shared/common/Config.h"
#include "shared/common/Visitor.h"

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Frontend/CompilerInstance.h"

namespace {

template <class... Types>
bool is_any_of(const clang::Stmt * val)
{
    return (... || clang::isa<Types>(val));
}
} // namespace

namespace ica {

InitMembersVisitor::InitMembersVisitor(clang::CompilerInstance & ci, const Config & checks)
    : Visitor(ci, checks)
{
    if (!isEnabled()) {
        return;
    }

    m_init_in_body_id = getCustomDiagID(init_members, "%0 can be constructed in initializer list");
}

bool InitMembersVisitor::VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * op_call)
{
    if (!(shouldProcessExpr(op_call, getSM()) && m_curr_ctor_body)) {
        return true;
    }

    if (!op_call->isAssignmentOp()) {
        return true;
    }
    checkAssignment(op_call->getArg(0), op_call->getArg(1));

    return true;
}

bool InitMembersVisitor::VisitBinaryOperator(clang::BinaryOperator * op_call)
{
    if (!(shouldProcessExpr(op_call, getSM()) && m_curr_ctor_body)) {
        return true;
    }

    if (!op_call->isAssignmentOp()) {
        return true;
    }
    checkAssignment(op_call->getLHS(), op_call->getRHS());
    return true;
}

bool InitMembersVisitor::VisitMemberExpr(clang::MemberExpr * member)
{
    if (!(shouldProcessExpr(member, getSM()) && m_curr_ctor_body)) {
        return true;
    }

    if (clang::isa<clang::CXXThisExpr>(member->getBase())) {
        foundMember(member);
    }
    return true;
}

bool InitMembersVisitor::VisitCXXConstructorDecl(clang::CXXConstructorDecl * ctor_decl)
{
    if (!(shouldProcessDecl(ctor_decl, getSM()) && ctor_decl->doesThisDeclarationHaveABody())) {
        return true;
    }

    m_curr_ctor_body = ctor_decl->getBody();
    return true;
}

bool InitMembersVisitor::dataTraverseStmtPost(clang::Stmt * stmt)
{
    if (stmt == m_curr_ctor_body) {
        m_curr_ctor_body = nullptr;
        for (auto member : m_to_report) {
            report(member->getExprLoc(), m_init_in_body_id)
                .AddValue(member->getMemberDecl());
        }
        m_to_report.clear();
    }
    return true;
}

void InitMembersVisitor::addToReport(const clang::MemberExpr * member)
{
    if (m_met_members.find(member) == m_met_members.end()) {
        m_to_report.insert(member);
    }
}

void InitMembersVisitor::foundMember(const clang::MemberExpr * member)
{
    m_met_members.insert(member);
}

namespace am = clang::ast_matchers;

AST_MATCHER(clang::DeclRefExpr, parmOrGlobal)
{
    auto var_decl = clang::dyn_cast<clang::VarDecl>(Node.getDecl());
    return var_decl && (clang::isa<clang::ParmVarDecl>(var_decl) || var_decl->hasGlobalStorage());
}

clang::StringRef nonParmOrGlobId()
{
    return clang::StringRef("non_parm_or_glob");
}

struct Callback : public am::MatchFinder::MatchCallback
{
    void run(const am::MatchFinder::MatchResult &)
    {
        was_called = true;
    }

    bool was_called = false;
};

void InitMembersVisitor::checkAssignment(const clang::Expr * lhs, const clang::Expr * rhs)
{
    auto member = clang::dyn_cast<clang::MemberExpr>(lhs);
    if (!member || !clang::isa<clang::CXXThisExpr>(member->getBase())) {
        return;
    }

    am::MatchFinder finder;
    Callback callback;
    finder.addMatcher(
        am::expr(
            am::forEachDescendant(
                am::declRefExpr(
                    am::unless(
                        parmOrGlobal()
                    )
                ).bind(nonParmOrGlobId())
            )
        ),
        &callback
    );
    finder.match(*rhs, getContext());

    if (!callback.was_called) {
        addToReport(member);
    }
}

} // namespace ica
