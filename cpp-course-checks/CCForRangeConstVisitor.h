#pragma once

#include "ICADiagnosticsBuilder.h"
#include "ICAVisitor.h"

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/multiset_of.hpp>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/StmtCXX.h>
#include <clang/Frontend/CompilerInstance.h>

class CCForRangeConstVisitor : public ICAVisitor<CCForRangeConstVisitor>
{
    static constexpr inline auto * for_range_const = "for-range-const";
    static constexpr inline auto * const_param = "const-param";

public:
    static constexpr inline auto check_names = make_check_names(for_range_const, const_param);

public:
    CCForRangeConstVisitor(clang::CompilerInstance & ci, const ICAConfig & checks);

    bool VisitCXXForRangeStmt(clang::CXXForRangeStmt * for_stmt);
    bool VisitFunctionDecl(clang::FunctionDecl * func_decl);

    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * member_call);
    bool VisitCallExpr(clang::CallExpr * call_expr);
    bool VisitBinaryOperator(clang::BinaryOperator * bin_op);
    bool VisitUnaryOperator(clang::UnaryOperator * un_op);
    bool VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * op_call);

    bool dataTraverseStmtPost(clang::Stmt * stmt);

    void printDiagnostic(clang::ASTContext &) {}
    void clear() {};

    using LoopVars = boost::bimaps::bimap<boost::bimaps::multiset_of<const clang::Stmt *>, const clang::ValueDecl *>;
    using LoopVarEntry = LoopVars::value_type;

private:
    void removeLoopVar(const clang::DeclRefExpr * loop_var);
    bool hasConstOverload(const clang::CXXMethodDecl * method_decl);

private:
    LoopVars m_loop_vars;

    std::unordered_map<const clang::CXXMethodDecl *, bool> m_has_const_overload;
    std::unordered_map<const clang::FunctionDecl *, std::vector<uint>> m_non_templated_params;

    DiagnosticID m_for_range_const_id = 0;
    DiagnosticID m_const_param_id = 0;
    DiagnosticID m_const_rvalue_id = 0;
};