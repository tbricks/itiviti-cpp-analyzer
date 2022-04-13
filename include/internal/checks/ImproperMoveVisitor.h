#pragma once

#include "shared/common/DiagnosticsBuilder.h"
#include "shared/common/Visitor.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"

#include <unordered_map>
#include <unordered_set>

namespace ica {

class ImproperMoveVisitor : public Visitor<ImproperMoveVisitor>
{
    static constexpr auto * improper_move = "improper-move";

public:
    static constexpr inline auto check_names = make_check_names(improper_move);

public:
    ImproperMoveVisitor(clang::CompilerInstance & ci, const Config & m_checks);

    bool VisitFunctionDecl(clang::FunctionDecl * func_decl);
    bool VisitCallExpr(clang::CallExpr * call);

    bool dataTraverseStmtPre(clang::Stmt * stmt);
    bool dataTraverseStmtPost(clang::Stmt * stmt);

    void printDiagnostic(clang::ASTContext & ctx) {}
    void clear() {}

public:

    struct ParamTraits
    {
        const clang::ParmVarDecl * parm_decl;
        bool is_templated;
        bool is_non_const_lvalue;
    };

    using Parameters = std::vector<ParamTraits>;
    using ParamsStack = std::vector<std::pair<const clang::Stmt *, Parameters>>; // lambdas may cause folded function definitions
    using MaybeParamTraits = std::optional<ParamTraits>;

public:
    MaybeParamTraits findParameter(const clang::ParmVarDecl * parm);
    bool isNotTemplateParam(const clang::ParmVarDecl * parm);
    bool isLvalueParam(const clang::ParmVarDecl * parm);
    void rvalueParamMoved(const clang::ParmVarDecl * parm);

private:
    std::unordered_map<const clang::FunctionDecl *, std::vector<uint>> m_non_template_params;
    ParamsStack m_params_stack;

    const clang::ImplicitCastExpr * m_last_const_cast = nullptr;

    DiagnosticID m_move_lvalue_id = 0;
    DiagnosticID m_move_rvalue_id = 0;
    DiagnosticID m_move_to_const_id = 0;
    DiagnosticID m_move_trivial_id = 0;
    DiagnosticID m_const_rvalue_id = 0;
};

} // namespace ica
