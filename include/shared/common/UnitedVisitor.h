#pragma once

#include "shared/common/Visitor.h"
#include "shared/common/Config.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/StmtCXX.h>
#include <memory>
#include <tuple>
#include <string_view>
#include <array>

namespace ica {

template<class ... Visitors>
class UnitedVisitor : public clang::RecursiveASTVisitor<UnitedVisitor<Visitors...>>
{
public:

    UnitedVisitor(clang::CompilerInstance & ci, const Config & config) :
        m_united_visitor(Visitors(ci, config)...),
        m_should_visit_template_instantiations(std::apply([](auto & ... visitors)
                {return ((visitors.isEnabled() && visitors.shouldVisitTemplateInstantiations()) || ...); }, m_united_visitor))
    {
    }

// use arithmetic 'or' to avoid short-circuit of logical operator
#define DEFINE_VISIT_METHOD(type) \
bool Visit ## type(clang::type * expr) \
{ \
    return std::apply([&expr](auto & ... visitors) \
        { return ((visitors.isEnabled() && visitors.Visit ## type(expr)) | ...); }, m_united_visitor); \
}

DEFINE_VISIT_METHOD(CallExpr)
DEFINE_VISIT_METHOD(ForStmt)
DEFINE_VISIT_METHOD(CompoundStmt)
DEFINE_VISIT_METHOD(CXXConstCastExpr)
DEFINE_VISIT_METHOD(CXXConstructExpr)
DEFINE_VISIT_METHOD(CXXConstructorDecl)
DEFINE_VISIT_METHOD(CXXForRangeStmt)
DEFINE_VISIT_METHOD(CXXMemberCallExpr)
DEFINE_VISIT_METHOD(VarDecl)
DEFINE_VISIT_METHOD(CXXOperatorCallExpr)
DEFINE_VISIT_METHOD(DeclRefExpr)
DEFINE_VISIT_METHOD(CXXRecordDecl)
DEFINE_VISIT_METHOD(ClassTemplateDecl)
DEFINE_VISIT_METHOD(BinaryOperator)
DEFINE_VISIT_METHOD(LambdaExpr)
DEFINE_VISIT_METHOD(FunctionDecl)
DEFINE_VISIT_METHOD(ReturnStmt)

#undef DEFINE_VISIT_METHOD

    void printDiagnostic(clang::ASTContext & context)
    {
        std::apply([&context](auto & ... visitors)
            { (printVisitorDiagnostic(context, visitors), ...); }, m_united_visitor);
    }

    void setContext(clang::ASTContext & context)
    {
        std::apply([&context](auto & ... visitors)
            { (setVisitorContext(context, visitors), ...); }, m_united_visitor);
    };

    bool shouldVisitTemplateInstantiations() const
    { return m_should_visit_template_instantiations; }

    bool dataTraverseStmtPre(clang::Stmt * s) // TODO: handle case when a visitor returns false
    {
        std::apply([&s](auto & ... visitors){ ((visitors.isEnabled() && visitors.dataTraverseStmtPre(s)), ...); }, m_united_visitor);
        return true;
    }

    bool dataTraverseStmtPost(clang::Stmt * s) // TODO: handle case when a visitor returns false
    {
        std::apply([&s](auto & ... visitors){ ((visitors.isEnabled() && visitors.dataTraverseStmtPost(s)), ...); }, m_united_visitor);
        return true;
    }

public:

    bool isEnabled()
    { return true; }

    void clear()
    {
        std::apply([](auto & ... visitors)
            { (clearVisitor(visitors), ...); }, m_united_visitor);
    }

private:

    template <class Visitor>
    static void printVisitorDiagnostic(clang::ASTContext & context, Visitor & visitor)
    {
        if (visitor.isEnabled()) {
            visitor.printDiagnostic(context);
        }
    }

    template <class Visitor>
    static void setVisitorContext(clang::ASTContext & context, Visitor & visitor)
    {
        if (visitor.isEnabled()) {
            visitor.setContext(context);
        }
    }
    template <class Visitor>
    static void clearVisitor(Visitor & visitor)
    {
        if (visitor.isEnabled()) {
            visitor.clear();
        }
    }

    std::tuple<Visitors...> m_united_visitor;
    bool m_should_visit_template_instantiations;
};

} // namespace ica
