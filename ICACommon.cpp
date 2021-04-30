#include "ICACommon.h"

bool isIdenticalStmt(
        const clang::ASTContext & ctx,
        const clang::Stmt * stmt1,
        const clang::Stmt * stmt2,
        const bool ignore_side_effects)
{
    using namespace clang;

    if (!stmt1 || !stmt2) {
        return !stmt1 && !stmt2;
    }

    // If stmt1 & stmt2 are of different class then they are not
    // identical statements.
    if (stmt1->getStmtClass() != stmt2->getStmtClass()) {
        return false;
    }

    const Expr * expr1 = dyn_cast<Expr>(stmt1);
    const Expr * expr2 = dyn_cast<Expr>(stmt2);

    if (expr1 && expr2) {
        // If stmt1 has side effects then don't warn even if expressions
        // are identical.
        if (!ignore_side_effects && expr1->HasSideEffects(ctx)) {
            return false;
        }
        // If either expression comes from a macro then don't warn even if
        // the expressions are identical.
        if ((expr1->getExprLoc().isMacroID()) || (expr2->getExprLoc().isMacroID())) {
            return false;
        }

        auto identical_child = [&ctx, ignore_side_effects] (const auto & child1, const auto & child2) {
            return child1 && child2 && isIdenticalStmt(ctx, child1, child2, ignore_side_effects);
        };

        const bool identical_children = std::equal(
                expr1->child_begin(), expr1->child_end(),
                expr2->child_begin(), expr2->child_end(),
                identical_child);

        if (!identical_children) {
            return false;
        }
    }

    switch (stmt1->getStmtClass()) {
    default:
        return false;
    case Stmt::CallExprClass:
    case Stmt::CXXMemberCallExprClass:
    case Stmt::CXXOperatorCallExprClass:
    case Stmt::ArraySubscriptExprClass:
    case Stmt::OMPArraySectionExprClass:
    case Stmt::ImplicitCastExprClass:
    case Stmt::ParenExprClass:
    case Stmt::BreakStmtClass:
    case Stmt::ContinueStmtClass:
    case Stmt::NullStmtClass:
    case Stmt::CXXThisExprClass:
        return true;
    case Stmt::CStyleCastExprClass: {
        const auto * cast_expr1 = cast<CStyleCastExpr>(stmt1);
        const auto * cast_expr2 = cast<CStyleCastExpr>(stmt2);

        return cast_expr1->getTypeAsWritten() == cast_expr2->getTypeAsWritten();
    }
    case Stmt::ReturnStmtClass: {
        const auto * return_stmt1 = cast<ReturnStmt>(stmt1);
        const auto * return_stmt2 = cast<ReturnStmt>(stmt2);

        return isIdenticalStmt(ctx, return_stmt1->getRetValue(), return_stmt2->getRetValue(), ignore_side_effects);
    }
    case Stmt::ForStmtClass: {
        const auto * for_stmt1 = cast<ForStmt>(stmt1);
        const auto * for_stmt2 = cast<ForStmt>(stmt2);

        return    isIdenticalStmt(ctx, for_stmt1->getInit(), for_stmt2->getInit(), ignore_side_effects)
               && isIdenticalStmt(ctx, for_stmt1->getCond(), for_stmt2->getCond(), ignore_side_effects)
               && isIdenticalStmt(ctx, for_stmt1->getInc() , for_stmt2->getInc() , ignore_side_effects)
               && isIdenticalStmt(ctx, for_stmt1->getBody(), for_stmt2->getBody(), ignore_side_effects);
    }
    case Stmt::DoStmtClass: {
        const auto * do_stmt1 = cast<DoStmt>(stmt1);
        const auto * do_stmt2 = cast<DoStmt>(stmt2);

        return    isIdenticalStmt(ctx, do_stmt1->getCond(), do_stmt2->getCond(), ignore_side_effects)
               && isIdenticalStmt(ctx, do_stmt1->getBody(), do_stmt2->getBody(), ignore_side_effects);
    }
    case Stmt::WhileStmtClass: {
        const auto * while_stmt1 = cast<WhileStmt>(stmt1);
        const auto * while_stmt2 = cast<WhileStmt>(stmt2);

        return    isIdenticalStmt(ctx, while_stmt1->getCond(), while_stmt2->getCond(), ignore_side_effects)
               && isIdenticalStmt(ctx, while_stmt1->getBody(), while_stmt2->getBody(), ignore_side_effects);
    }
    case Stmt::IfStmtClass: {
        const IfStmt * if_stmt1 = cast<IfStmt>(stmt1);
        const IfStmt * if_stmt2 = cast<IfStmt>(stmt2);

        return    isIdenticalStmt(ctx, if_stmt1->getCond(), if_stmt2->getCond(), ignore_side_effects)
               && isIdenticalStmt(ctx, if_stmt1->getThen(), if_stmt2->getThen(), ignore_side_effects)
               && isIdenticalStmt(ctx, if_stmt1->getElse(), if_stmt2->getElse(), ignore_side_effects);
    }
    case Stmt::CompoundStmtClass: {
        const CompoundStmt * comp_stmt1 = cast<CompoundStmt>(stmt1);
        const CompoundStmt * comp_stmt2 = cast<CompoundStmt>(stmt2);

        if (comp_stmt1->size() != comp_stmt2->size())
            return false;

        auto identical_body = [&ctx, ignore_side_effects] (const auto & body1, const auto & body2) {
            return isIdenticalStmt(ctx, body1, body2, ignore_side_effects);
        };

        return std::equal(
                comp_stmt1->body_begin(), comp_stmt1->body_end(),
                comp_stmt2->body_begin(), comp_stmt2->body_end(),
                identical_body);
    }
    case Stmt::CompoundAssignOperatorClass:
    case Stmt::BinaryOperatorClass: {
        const auto * bin_op1 = cast<BinaryOperator>(stmt1);
        const auto * bin_op2 = cast<BinaryOperator>(stmt2);

        return bin_op1->getOpcode() == bin_op2->getOpcode();
    }
    case Stmt::CharacterLiteralClass: {
        const auto * char_lit1 = cast<CharacterLiteral>(stmt1);
        const auto * char_lit2 = cast<CharacterLiteral>(stmt2);

        return char_lit1->getValue() == char_lit2->getValue();
    }
    case Stmt::DeclRefExprClass: {
        const auto *decl_ref1 = cast<DeclRefExpr>(stmt1);
        const auto *decl_ref2 = cast<DeclRefExpr>(stmt2);

        return decl_ref1->getDecl() == decl_ref2->getDecl();
    }
    case Stmt::IntegerLiteralClass: {
        const auto * int_lit1 = cast<IntegerLiteral>(stmt1);
        const auto * int_lit2 = cast<IntegerLiteral>(stmt2);

        llvm::APInt I1 = int_lit1->getValue();
        llvm::APInt I2 = int_lit2->getValue();

        return    (I1.getBitWidth() == I2.getBitWidth())
               && (I1 == I2);
        }
    case Stmt::FloatingLiteralClass: {
        const auto * float_lit1 = cast<FloatingLiteral>(stmt1);
        const auto * float_lit2 = cast<FloatingLiteral>(stmt2);

        return float_lit1->getValue().bitwiseIsEqual(float_lit2->getValue());

    }
    case Stmt::StringLiteralClass: {
        const auto * string_lit1 = cast<StringLiteral>(stmt1);
        const auto * string_lit2 = cast<StringLiteral>(stmt2);

        return string_lit1->getBytes() == string_lit2->getBytes();
    }
    case Stmt::MemberExprClass: {
        const auto * member_stmt1 = cast<MemberExpr>(stmt1);
        const auto * member_stmt2 = cast<MemberExpr>(stmt2);

        return member_stmt1->getMemberDecl() == member_stmt2->getMemberDecl();
    }
    case Stmt::UnaryOperatorClass: {
       const auto * unary_op1 = cast<UnaryOperator>(stmt1);
       const auto * unary_op2 = cast<UnaryOperator>(stmt2);

       return unary_op1->getOpcode() == unary_op2->getOpcode();
    }
    }
}


std::string getCheckURL(const std::string_view check_name)
{
    return "https://github.com/Kurkin/ica-lint-action#"
        + std::string(check_name);
}

std::string wrapCheckNameWithURL(const std::string_view check_name)
{
    // can conditionally wrap depending on terminal
    return "\x1B]8;;" + getCheckURL(check_name) + "\x1B\\" + std::string(check_name) + "\x1B]8;;\x1B\\";
}

const clang::DeclRefExpr * extractDeclRef(const clang::Expr * expr)
{
    expr = expr->IgnoreImpCasts();
    if (auto arr_subscr = clang::dyn_cast<clang::ArraySubscriptExpr>(expr)) {
        expr = arr_subscr->getBase();
    }
    else if (auto mem_call = clang::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
        auto method_decl = mem_call->getMethodDecl();
        if (method_decl->getReturnType()->isReferenceType()) { // we associate method result with the object itself only if it returns a reference
            expr = mem_call->getImplicitObjectArgument();
        }
    }
    else if (auto mem_expr = clang::dyn_cast<clang::MemberExpr>(expr)) {
        expr = mem_expr->getBase();
    }
    else if (auto op_call = clang::dyn_cast<clang::CXXOperatorCallExpr>(expr)) {
        expr = op_call->getArg(0); // operator has at least one argument, I'm pretty sure it's the argument, operator applied to
    }
    return clang::dyn_cast<clang::DeclRefExpr>(expr->IgnoreCasts());
}