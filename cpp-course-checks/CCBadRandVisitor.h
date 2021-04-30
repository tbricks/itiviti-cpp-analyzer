#pragma once

#include "ICAConfig.h"
#include "ICADiagnosticsBuilder.h"
#include "ICAVisitor.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/Stmt.h>
#include <clang/Frontend/CompilerInstance.h>

namespace mi = boost::multi_index;

class CCBadRandVisitor : public ICAVisitor<CCBadRandVisitor>
{
    static constexpr inline auto bad_rand = "bad-rand";

public:
    static constexpr inline auto check_names = make_check_names(bad_rand);

public:
    CCBadRandVisitor(clang::CompilerInstance & ci, const ICAConfig & checks);

    bool VisitCallExpr(clang::CallExpr * call);
    bool VisitCXXConstructExpr(clang::CXXConstructExpr * constr);
    bool VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * op_call);
    bool VisitVarDecl(clang::VarDecl * var_decl);
    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * method_call);
    bool VisitCXXConstructorDecl(clang::CXXConstructorDecl * ctor_decl);

    bool dataTraverseStmtPre(clang::Stmt * stmt);
    bool dataTraverseStmtPost(clang::Stmt * stmt);

    void printDiagnostic(clang::ASTContext &) {};
    void clear() {}

    struct RandomEngine
    {
        const clang::VarDecl * engn_decl;
        const clang::CompoundStmt * comp_stmt;
    };

    struct EngnDecl;
    struct CompStmt;

    using RandomEnginesTable = mi::multi_index_container<
        RandomEngine,
        mi::indexed_by<
            mi::hashed_unique<mi::tag<EngnDecl>, mi::member<RandomEngine, const clang::VarDecl *, &RandomEngine::engn_decl>>,
            mi::hashed_non_unique<mi::tag<CompStmt>, mi::member<RandomEngine, const clang::CompoundStmt *, &RandomEngine::comp_stmt>>
        >
    >;
    using RandomEnginesEntry = RandomEnginesTable::value_type;
private:
    RandomEnginesTable m_rand_engns;
    std::vector<const clang::CompoundStmt *> m_comp_stmt_stack;
    std::vector<const clang::Stmt *> m_ctor_body_stack;

    DiagnosticID m_rand_id = 0;
    DiagnosticID m_random_shuffle_id = 0;
    DiagnosticID m_engine_construction_id = 0;
    DiagnosticID m_one_time_usage_id = 0;
    DiagnosticID m_use_distribution_id = 0;
    DiagnosticID m_slow_random_device_id = 0;

    uint m_cycle_depth = 0;
    bool m_is_defining_seed = false;
};