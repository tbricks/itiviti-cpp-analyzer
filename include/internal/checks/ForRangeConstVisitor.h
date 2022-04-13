#pragma once

#include "shared/common/Common.h"
#include "shared/common/DiagnosticsBuilder.h"
#include "shared/common/Visitor.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/Frontend/CompilerInstance.h"

#include "boost/multi_index/member.hpp"
#include "boost/multi_index/ordered_index.hpp"
#include "boost/multi_index/tag.hpp"
#include "boost/multi_index_container.hpp"
#include "boost/range/iterator_range.hpp"

#include <cstdint>

namespace ica {

namespace mi = boost::multi_index;

class ForRangeConstVisitor : public Visitor<ForRangeConstVisitor>
{
    static constexpr inline auto * for_range_const = "for-range-const";
    static constexpr inline auto * const_param = "const-param";

public:
    static constexpr inline auto check_names = make_check_names(for_range_const, const_param);

public:
    ForRangeConstVisitor(clang::CompilerInstance & ci, const Config & checks);
    ForRangeConstVisitor(ForRangeConstVisitor &&);
    ~ForRangeConstVisitor();

    bool VisitCXXForRangeStmt(clang::CXXForRangeStmt * for_stmt);
    bool VisitFunctionDecl(clang::FunctionDecl * func_decl);
    bool VisitVarDecl(clang::VarDecl * var_decl);

    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * member_call);
    bool VisitCallExpr(clang::CallExpr * call_expr);
    bool VisitBinaryOperator(clang::BinaryOperator * bin_op);
    bool VisitUnaryOperator(clang::UnaryOperator * un_op);
    bool VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * op_call);
    bool VisitReturnStmt(clang::ReturnStmt * ret_stmt);

    bool dataTraverseStmtPost(clang::Stmt * stmt);

    void printDiagnostic(clang::ASTContext &) {}
    void clear() {};

private:
    // function/cycle body <-> parameter
    struct CheckedVar
    {
        CheckedVar(const clang::Stmt * scope, const clang::ValueDecl * self)
            : scope(scope)
            , self(self)
            , referred(self)
        {}

        CheckedVar(const clang::Stmt * scope, const clang::ValueDecl * self, const clang::ValueDecl * referred)
            : scope(scope)
            , self(self)
            , referred(referred)
        {}

        CheckedVar referBy(const clang::ValueDecl * var) { return CheckedVar{this->scope, var, this->referred}; }
        bool isReferred() const { return self == referred; }

        const clang::Stmt * scope;
        const clang::ValueDecl * self;
        /// the value that 'self' refers to
        const clang::ValueDecl * referred = nullptr; // we want to check not only func/loop parameter,
                                                     // but also references to (or iterators of) it
    };

    struct Scope {};
    struct Referred {};
    struct Var {};

    using CheckedVars = mi::multi_index_container<
        CheckedVar,
        mi::indexed_by<
            mi::ordered_non_unique<mi::tag<Scope>, mi::member<CheckedVar, const clang::Stmt *, &CheckedVar::scope>>,
            mi::ordered_non_unique<mi::tag<Referred>, mi::member<CheckedVar, const clang::ValueDecl *, &CheckedVar::referred>>,
            mi::ordered_unique<mi::tag<Var>, mi::member<CheckedVar, const clang::ValueDecl *, &CheckedVar::self>>
        >
    >;
    using CheckedVarsIter = CheckedVars::iterator;

    struct SubExpr
    {

        const clang::Expr * self;
        const clang::Expr * parent = nullptr;
        /// a value that is initialized by 'self' expression
        const clang::ValueDecl * initialized = nullptr;
    };

    struct SubExprCmp
    {
        using is_transparent = void;

        bool operator()(const SubExpr & lhs, const SubExpr & rhs) const { return lhs.self < rhs.self; }
        bool operator()(const clang::Expr * lhs, const SubExpr & rhs) const { return lhs < rhs.self; }
        bool operator()(const SubExpr & lhs, const clang::Expr * rhs) const { return lhs.self < rhs; }
    };

private:
    void removeCheckedVar(const clang::DeclRefExpr * checked_var);
    std::optional<CheckedVar> asCheckedVar(const clang::DeclRefExpr * var);
    bool hasConstOverload(const clang::CXXMethodDecl * method_decl);
    RefTypeInfo getRefTypeInfo(const clang::QualType & type);
    bool isMutRef(const clang::QualType & type);

    void addMutRefProducer(const clang::Expr * expr, const clang::Expr * prnt, const clang::ValueDecl * intied = nullptr);
    bool producesMutRef(const clang::Expr * expr);

private:
    struct FunctionDeclStack;

private:
    CheckedVars m_checked_vars;

    std::unordered_map<const clang::CXXMethodDecl *, bool> m_has_const_overload;
    std::unordered_map<const clang::FunctionDecl *, std::vector<uint8_t>> m_non_templated_params;
    std::unique_ptr<FunctionDeclStack> m_function_decl_stack;

    /** thoughts:
        1. actually, lots of different expressions may be references
            (e.g. *(vec.begin() + 2), make_optional(&par))
        2. we traverse the AST in top-to-bottom order
       idea:
        While AST traversal instead of checking that X itself is parameter or reference,
         let's consider it as a _potential_ reference
        If we descend to an actual reference, then we can say, that it was referred in
         the expression as a whole
        This way we may cover much more ways to refer the parameter to check possibility
         of it constness
    **/
    std::set<SubExpr, SubExprCmp> m_mb_refs;
    MemorizingFunctor<decltype(&isIterator)> m_is_iterator{&isIterator};

    DiagnosticID m_for_range_const_id = 0;
    DiagnosticID m_const_param_id = 0;
    DiagnosticID m_const_rvalue_id = 0;
};

} // namespace ica
