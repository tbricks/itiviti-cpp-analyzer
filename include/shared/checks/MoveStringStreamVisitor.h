#pragma once

#include "shared/common/Visitor.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace ica {

class MoveStringStreamVisitor : public Visitor<MoveStringStreamVisitor>
{
    using ExprList = std::vector<const clang::Expr *>;
    using DeclToExprList = std::map<const clang::Decl *, ExprList>;

    static constexpr inline auto * move_string_stream = "move-string-stream";

public:
    explicit MoveStringStreamVisitor(clang::CompilerInstance & ci, const Config & config);

public:
    static constexpr inline auto check_names = make_check_names(move_string_stream);

public:
    bool VisitVarDecl(clang::VarDecl * decl);
    bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * expr);
    bool VisitDeclRefExpr(clang::DeclRefExpr * expr);

    void clear();
    void printDiagnostic(clang::ASTContext & context);

private:
    DeclToExprList m_all_references_to_string_streams;
    DeclToExprList m_decl_refs_to_string_streams_on_str_calls;

    DiagnosticID m_warn_id = 0;
};

} // namespace ica
