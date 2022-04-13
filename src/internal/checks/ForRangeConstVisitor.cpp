#include "internal/checks/ForRangeConstVisitor.h"

#include "shared/common/Common.h"
#include "shared/common/Config.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/Type.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/OperatorKinds.h"
#include "clang/Basic/Specifiers.h"
#include "clang/Frontend/CompilerInstance.h"

#include <algorithm>
#include <numeric>
#include <string_view>

namespace {

bool isConstType(const clang::QualType & type)
{
    auto derefed = type.getNonReferenceType();
    return derefed->isPointerType() ? derefed->getPointeeType().isConstQualified() : derefed.isConstQualified();
}

bool isRefOrPtr(const clang::QualType & type)
{
    return type->isReferenceType() || type->isPointerType();
}

bool sameParams(const clang::FunctionDecl * l_func, const clang::FunctionDecl * r_func)
{
    return l_func->getNumParams() == r_func->getNumParams() &&
            std::equal(l_func->param_begin(), l_func->param_end(), r_func->param_begin(), [](const clang::ParmVarDecl * l_param, const clang::ParmVarDecl * r_param) {
                return l_param->getType() == r_param->getType();
            });
}

const clang::Expr * nullIfNotConsidered(const clang::Expr * expr)
{
    if (!expr) {
        return nullptr;
    }

    expr = expr->IgnoreParenCasts();
    const bool is_considered = clang::isa<clang::DeclRefExpr>(expr)    ||
                               clang::isa<clang::CallExpr>(expr)       ||
                               clang::isa<clang::BinaryOperator>(expr) ||
                               clang::isa<clang::UnaryOperator>(expr)  ||
                               clang::isa<clang::MemberExpr>(expr);
    return is_considered ? expr : nullptr;
}

} // namespace

namespace ica {

struct ForRangeConstVisitor::FunctionDeclStack
{
    bool push_if_has_body(const clang::FunctionDecl * func_decl)
    {
        if (func_decl && func_decl->doesThisDeclarationHaveABody()) {
            m_stack.push_back(func_decl);
            return true;
        }
        return false;
    }

    bool empty() const noexcept { return m_stack.empty(); }
    clang::QualType curr_ret_type() const { return m_stack.back()->getReturnType(); }

    void end_body(const clang::Stmt * stmt) {
        if (empty()) {
            return;
        }
        if (stmt == m_stack.back()->getBody()) {
            m_stack.pop_back();
        }
    }

private:
    std::vector<const clang::FunctionDecl *> m_stack;
};


ForRangeConstVisitor::ForRangeConstVisitor(clang::CompilerInstance & ci, const Config & checks)
    : Visitor(ci, checks)
    , m_function_decl_stack(std::make_unique<FunctionDeclStack>())
{
    if (!isEnabled()) return;

    m_for_range_const_id = getCustomDiagID(for_range_const, "'const' should be specified explicitly for variable in for-range loop");
    m_const_param_id = getCustomDiagID(const_param, "%0 can have 'const' qualifier");
    m_const_rvalue_id = getCustomDiagID(const_param, "%0 is got as rvalue-reference, but never modified");
}

ForRangeConstVisitor::ForRangeConstVisitor(ForRangeConstVisitor &&) = default;

ForRangeConstVisitor::~ForRangeConstVisitor() = default;

bool ForRangeConstVisitor::VisitCXXForRangeStmt(clang::CXXForRangeStmt *for_stmt)
{
    if(!shouldProcessStmt(for_stmt, getSM())) {
        return true;
    }
    {
        auto * begin_stmt = for_stmt->getBeginStmt();
        if (begin_stmt) {
            for (auto * decl : begin_stmt->decls()) {
                VisitVarDecl(clang::dyn_cast_or_null<clang::VarDecl>(decl));
            }
        }
        auto * end_stmt =  for_stmt->getEndStmt();
        if (end_stmt) {
            for (auto * decl : end_stmt->decls()) {
                VisitVarDecl(clang::dyn_cast_or_null<clang::VarDecl>(decl));
            }
        }
    }

    auto loop_var = for_stmt->getLoopVariable();
    auto ref_info = getRefTypeInfo(loop_var->getType());
    if (ref_info.is_ref && !ref_info.is_const) {
        removeCheckedVar(extractDeclRef(for_stmt->getRangeInit()));
    }

    if (loop_var->getType()->isPointerType() || ref_info.is_const) {
        return true; // ignore already const variables or pointers
    }

    if (for_stmt->getRangeInit()->isInstantiationDependent()) {
        return true;
    }

    if (auto decomp_decl = clang::dyn_cast<clang::DecompositionDecl>(loop_var)) {
        for(const auto binding : decomp_decl->bindings()) {
            m_checked_vars.emplace(for_stmt, binding);
        }
    } else {
        m_checked_vars.emplace(for_stmt, loop_var);
    }

    return true;
}

bool ForRangeConstVisitor::VisitFunctionDecl(clang::FunctionDecl * func_decl)
{
    if (!shouldProcessDecl(func_decl, getSM())) {
        return true;
    }

    if (func_decl->isTemplated()) {
        auto [it, emplaced] = m_non_templated_params.try_emplace(func_decl);
        if (emplaced) {
            uint parm_num = 0;
            for (const auto * parm : func_decl->parameters()) {
                if (!parm->getType()->isInstantiationDependentType() && !parm->getType()->isPointerType()) {
                    it->second.push_back(parm_num);
                }
                ++parm_num;
            }
        }
        return true;
    }

    if (!m_function_decl_stack->push_if_has_body(func_decl)) {
        return true;
    }

    if (auto method_decl = clang::dyn_cast<clang::CXXMethodDecl>(func_decl)) {
        if (method_decl->isVirtual()) {
            return true;
        }
    }

    std::function<bool(uint)> not_templated = [](auto) { return true; };
    if (auto it = m_non_templated_params.find(func_decl->getTemplateInstantiationPattern()); it != m_non_templated_params.end()) {
        const auto & line = it->second;
        not_templated = [expected_idx(uint8_t(0)), &line](uint8_t parm_num) mutable {
            bool res = false;
            if (expected_idx < line.size()) {
                res = line[expected_idx] == parm_num;
            }
            expected_idx += res;
            return res;
        };
    }

    // find out which of function parameters are used to initialize a class member of reference type
    std::set<const clang::ParmVarDecl *> ref_initializers;
    if (auto as_ctor_decl = clang::dyn_cast<clang::CXXConstructorDecl>(func_decl)) {
        for (auto init_expr : as_ctor_decl->inits()) {
            if (auto member = init_expr->getAnyMember()) {
                if (!isMutRef(member->getType())) {
                    continue;
                }
                if (auto decl_ref = extractDeclRef(init_expr->getInit())) {
                    ref_initializers.insert(clang::dyn_cast<clang::ParmVarDecl>(decl_ref->getDecl()));
                }
            }
        }
    }
    auto inits_ref = [&ref_initializers](const clang::ParmVarDecl * parm) {
        return ref_initializers.find(parm) != ref_initializers.end();
    };

    const auto * body = func_decl->getBody();
    uint parm_num = 0;
    for (const auto * parm : func_decl->parameters()) {
        if (parm_num > 255) {
            llvm::outs() << "More than 256 parameters in " <<
                    func_decl->getNameInfo().getAsString() <<
                    ". for-range-const and const-param may have problems\n";
        }
        if (not_templated(parm_num) && !isConstType(parm->getType()) && parm->getType()->isReferenceType() && !inits_ref(parm)) {
            m_checked_vars.emplace(body, parm);
        }
        ++parm_num;
    }

    return true;
}

bool ForRangeConstVisitor::VisitVarDecl(clang::VarDecl *var_decl)
{
    if (!shouldProcessDecl(var_decl, getSM())) {
        return true;
    }

    if (!var_decl->isLocalVarDecl()) {
        return true;
    }

    if (!isMutRef(var_decl->getType())) {
        return true;
    }

    addMutRefProducer(var_decl->getInit(), nullptr, var_decl);
    return true;
}

bool ForRangeConstVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *member_call)
{
    if (m_checked_vars.empty()) {
        return true;
    }

    auto method_decl = member_call->getMethodDecl();
    if (!method_decl) {
        return true;
    }

    const auto * caller = member_call->getImplicitObjectArgument();
    if (producesMutRef(member_call) || !hasConstOverload(method_decl)) {
        addMutRefProducer(caller, member_call);
    }
    return true;
}

bool ForRangeConstVisitor::VisitCallExpr(clang::CallExpr *call)
{
    if (m_checked_vars.empty()) {
        return true;
    }

    if (auto callee = call->getDirectCallee()) {

        uint arg_num = 0;
        if (auto method = clang::dyn_cast<clang::CXXMethodDecl>(callee); method && clang::isa<clang::CXXOperatorCallExpr>(call)) {
            if (!hasConstOverload(method)) {
                addMutRefProducer(call->getArg(arg_num), call);
            }
            ++arg_num;
        }
        for (const auto * parm : callee->parameters()) {
            /* There are 2 cases:
                void foo(vector<int> & v) { sort(v.begin(), v.end()); }
                void boo(vector<int> & v) { for_each(v.begin(), v.end(), [](int){})}

               I didn't manage to check if 'v' is 'const'-able in the second one, but not the first,
                so in order to avoid false-positives I consider all templated functions as modifying
            */
            if (isMutRef(parm->getType()) || callee->getPrimaryTemplate()) {
                addMutRefProducer(call->getArg(arg_num), call);
            }
            ++arg_num;
        }
        for (auto arg_it = call->arg_begin() + arg_num; arg_it != call->arg_end(); ++arg_it) {
            if (getRefTypeInfo((*arg_it)->getType()).is_ref) {
                addMutRefProducer(call->getArg(arg_num), call);
            }
        }
    }

    return true;
}



bool ForRangeConstVisitor::VisitBinaryOperator(clang::BinaryOperator * bin_op)
{
    if (m_checked_vars.empty()) {
        return true;
    }

    if (bin_op->isAssignmentOp()) {
        addMutRefProducer(bin_op->getLHS(), bin_op);
    } else if (producesMutRef(bin_op) && isMutRef(bin_op->getType())) {
        addMutRefProducer(bin_op->getLHS(), bin_op);
        addMutRefProducer(bin_op->getRHS(), bin_op);
    }

    return true;
}

bool ForRangeConstVisitor::VisitUnaryOperator(clang::UnaryOperator *un_op)
{
    if (m_checked_vars.empty()) {
        return true;
    }

    auto kind = un_op->getOpcode();
    bool should_check_sub_expr = false;
    switch (kind) {
    case clang::UO_PostDec:
    case clang::UO_PostInc:
    case clang::UO_PreDec:
    case clang::UO_PreInc:
        should_check_sub_expr = true;
        break;
    case clang::UO_AddrOf:
    case clang::UO_Deref:
        should_check_sub_expr = producesMutRef(un_op);
        break;
    default:
        break;
    }

    if (should_check_sub_expr) {
        addMutRefProducer(un_op->getSubExpr(), un_op);
    }

    return true;
}

bool ForRangeConstVisitor::VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr * op_call)
{
    if (m_checked_vars.empty()) {
        return true;
    }

    if (const auto * func_decl = op_call->getDirectCallee()) {
        if (const auto * method_decl = clang::dyn_cast<clang::CXXMethodDecl>(func_decl)) {
            if (!hasConstOverload(method_decl) || producesMutRef(op_call)) {
                addMutRefProducer(op_call->getArg(0), op_call);
            }
        } else {
            if (auto lhs_type = func_decl->getParamDecl(0)->getType(); isMutRef(lhs_type)) {
                addMutRefProducer(op_call->getArg(0), op_call);
            }
        }
    }

    return true;
}

bool ForRangeConstVisitor::VisitReturnStmt(clang::ReturnStmt *ret_stmt)
{
    if (m_function_decl_stack->empty()) {
        return true;
    }

    auto curr_ret_type = m_function_decl_stack->curr_ret_type();
    if (isMutRef(curr_ret_type)) {
        addMutRefProducer(ret_stmt->getRetValue(), nullptr);
    }
    return true;
}

bool ForRangeConstVisitor::dataTraverseStmtPost(clang::Stmt *stmt)
{
    if (auto [begin, end] = m_checked_vars.get<Scope>().equal_range(stmt); begin != end && begin->scope == stmt) {
        if (const auto * for_stmt = clang::dyn_cast<clang::CXXForRangeStmt>(stmt)) {
            if (getCheck(for_range_const)) {
                report(for_stmt->getLoopVariable()->getTypeSpecStartLoc(), m_for_range_const_id);
            }
        } else if (getCheck(const_param)) {
            std::for_each(begin, end, [this](const auto & checked_entry) {
                if (checked_entry.isReferred()) {
                    const clang::ValueDecl * parm = checked_entry.referred;
                    report(parm->getBeginLoc(), parm->getType()->isRValueReferenceType() ? m_const_rvalue_id : m_const_param_id)
                        .AddValue(parm);
                }
            });
        }
        m_checked_vars.get<Scope>().erase(begin, end);
    }
    m_function_decl_stack->end_body(stmt);
    return true;
}

void ForRangeConstVisitor::removeCheckedVar(const clang::DeclRefExpr * checked_var)
{
    if (!checked_var) {
        return;
    }

    auto referred_it = m_checked_vars.get<Var>().find(checked_var->getDecl());
    if (referred_it == m_checked_vars.get<Var>().end()) {
        return;
    }

    if (clang::isa<clang::CXXForRangeStmt>(referred_it->scope)) {
        // if one of binded values is modified, can't declare whole deomposition as 'const'
        m_checked_vars.get<Scope>().erase(referred_it->scope);
    } else {
        m_checked_vars.get<Referred>().erase(referred_it->referred);
    }
}

auto ForRangeConstVisitor::asCheckedVar(const clang::DeclRefExpr * var) -> std::optional<CheckedVar>
{
    if (!var) {
        return std::nullopt;
    }

    auto & vars = m_checked_vars.get<Var>();
    auto var_it = vars.find(var->getDecl());
    if (var_it != vars.end()) {
        return std::make_optional(*var_it);
    }
    return std::nullopt;
}

bool ForRangeConstVisitor::hasConstOverload(const clang::CXXMethodDecl * method_decl)
{
    auto [cached_it, emplaced] = m_has_const_overload.try_emplace(method_decl, false);
    if (!emplaced) {
        return cached_it->second;
    }
    if (method_decl->isConst()) {
        return cached_it->second = true;
    }
    clang::CXXRecordDecl::method_range methods = method_decl->getParent()->methods();
    const clang::FunctionDecl * searched_func = method_decl;

    if (method_decl->isTemplateInstantiation()) {
        searched_func = method_decl->getTemplateInstantiationPattern();
        auto template_pattern = method_decl->getParent()->getTemplateInstantiationPattern();
        if (template_pattern) {
            methods = template_pattern->methods();
        }
    }

    auto accessible = [](const clang::CXXMethodDecl * other_func) {
        return other_func && other_func->isConst() && other_func->getAccess() == clang::AS_public;
    };
    auto same_params = [searched_func](const clang::FunctionDecl * other_func) { return sameParams(searched_func, other_func); };

    std::function<bool(const clang::CXXMethodDecl *)> same_name;
    if (auto identifier = searched_func->getIdentifier()) {
        same_name = [name(identifier->getName())](const auto * other_func) {
            if (auto identifier = other_func->getIdentifier()) {
                return identifier->getName() == name;
            }
            return false;
        };
    } else if (auto conversion = clang::dyn_cast<clang::CXXConversionDecl>(searched_func)) {
        same_name = [type(conversion->getConversionType())](const auto * other_func) {
            if (auto conversion = clang::dyn_cast<clang::CXXConversionDecl>(other_func)) {
                return conversion->getConversionType() == type;
            }
            return false;
        };
    } else if (searched_func->isOverloadedOperator()) {
        same_name = [op_kind(searched_func->getOverloadedOperator())](const auto * other_func) {
            return other_func->isOverloadedOperator() && other_func->getOverloadedOperator() == op_kind;
        };
    } else {
        same_name = [](const auto *) {
            return false;
        };
    }

    auto same_but_const = [&accessible, &same_name, &same_params](const clang::CXXMethodDecl * other_func) {
        return accessible(other_func) && same_name(other_func) && same_params(other_func);
    };

    return cached_it->second = std::any_of(methods.begin(), methods.end(), same_but_const);
}

RefTypeInfo ForRangeConstVisitor::getRefTypeInfo(const clang::QualType & type)
{
    auto iter_info = m_is_iterator(type.getUnqualifiedType()->getAsCXXRecordDecl());
    if (iter_info.is_ref) {
        return iter_info;
    } else {
        return {isRefOrPtr(type), isConstType(type)};
    }
}

bool ForRangeConstVisitor::isMutRef(const clang::QualType & type)
{
    auto ref_type_info = getRefTypeInfo(type);
    return ref_type_info.is_ref && !ref_type_info.is_const;
}

void ForRangeConstVisitor::addMutRefProducer(const clang::Expr * expr, const clang::Expr * prnt, const clang::ValueDecl * inited)
{
    expr = nullIfNotConsidered(expr);
    if (expr) {
        const auto * curr_sub_expr = &*m_mb_refs.insert({expr, prnt, inited}).first;

        do {
            const auto * dr_expr = extractDeclRef(curr_sub_expr->self);
            removeCheckedVar(dr_expr);
            auto mb_checked_var = asCheckedVar(dr_expr);
            if (mb_checked_var) {
                auto & checked_var = *mb_checked_var;
                m_checked_vars.insert(checked_var.referBy(curr_sub_expr->initialized));
            }

            if (curr_sub_expr->parent) {
                auto found_it = m_mb_refs.find(curr_sub_expr->parent);
                if (found_it != m_mb_refs.end()) {
                    curr_sub_expr = &*found_it;
                } else {
                    curr_sub_expr = nullptr;
                }
            } else {
                curr_sub_expr = nullptr;
            }
        } while(curr_sub_expr);
    }
}

bool ForRangeConstVisitor::producesMutRef(const clang::Expr * expr)
{ return m_mb_refs.count(nullIfNotConsidered(expr)) > 0; }

} // namespace ica
