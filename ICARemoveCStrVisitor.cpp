#include "ICARemoveCStrVisitor.h"
#include "ICACommon.h"

ICARemoveCStrVisitor::ICARemoveCStrVisitor(clang::CompilerInstance & ci, const ICAConfig & config)
    : ICAVisitor(ci, config)
{
    if (!isEnabled()) return;

    m_replace_method_id = getCustomDiagID(remove_c_str, "call to c_str can be removed, overload with %0 available");
    m_implicit_cast_id = getCustomDiagID(remove_c_str, "call to c_str can be removed, implicit cast from std::string is possible");

    m_note_id = getCustomDiagID(clang::DiagnosticIDs::Note,
            "overload defined here");
}

enum class RefStringType
{
    None,
    StringRef,
    StringView,
};

namespace {

std::pair<bool, RefStringType> is_string_ref_parameter(const clang::QualType & type, clang::ASTContext & context)
{
    if (!type.isConstQualified() && type->isReferenceType()) {
        return {false, RefStringType::None};
    }

    auto unqual_name = getUnqualifiedClassName(type);

    if (unqual_name == "tbricks::types::StringRef") {
        return {true, RefStringType::StringRef};
    }
    if (unqual_name == "std::basic_string_view<char, std::char_traits<char> >") {
        return {true, RefStringType::StringView};
    }
    return {false, RefStringType::None};
}

}

bool ICARemoveCStrVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr * member_call)
{
    using namespace std::literals::string_view_literals;

    if (!member_call ||
            isExpansionInSystemHeader(member_call, getSM()) ||
            isInvalidLocation(member_call) ||
            isNolintLocation(member_call, getSM())) {
        return true;
    }

    auto * our_method_decl = member_call->getMethodDecl();
    if (!our_method_decl) {
        return true;
    }

    auto * record_decl = member_call->getRecordDecl();
    if (!record_decl) {
        return true;
    }

    int arg_num = -1;
    for (auto arg : member_call->arguments()) {
        arg_num++;
        bool is_impl_cast = false;

        if (auto impl_cast = clang::dyn_cast<clang::ImplicitCastExpr>(arg)) {
            if (auto [is_string_ref, _] = is_string_ref_parameter(impl_cast->getType(), getContext()); is_string_ref) {
                if (auto ctor_expr = clang::dyn_cast<clang::CXXConstructExpr>(impl_cast->getSubExpr()); ctor_expr && ctor_expr->getNumArgs() > 0) {
                    is_impl_cast = true;
                    arg = ctor_expr->getArg(0);
                }
            }
        }

        auto * arg_member_call = clang::dyn_cast<clang::CXXMemberCallExpr>(arg);
        if (!arg_member_call) {
            continue;
        }

        auto * arg_method_decl = arg_member_call->getMethodDecl();
        if (!arg_method_decl || arg_method_decl->getNameInfo().getAsString() != "c_str") {
            continue;
        }

        auto * arg_record_decl = arg_member_call->getRecordDecl();
        if (!arg_record_decl || arg_record_decl->getName() != "basic_string"sv) {
            continue;
        }

        // special hack for DateTimeFactory due to its poor implementation
        if (record_decl->getName() == "DateTimeFactory"sv) {
            return true;
        }

        if (is_impl_cast) {
            // at this point we know that implicit cast from 'const char *' to 'std::string_view' took place
            // and that it was got from 'std::string::c_str' method
            report(arg_member_call->getExprLoc(), m_implicit_cast_id);
            continue;
        }

        for (const auto * other_method : record_decl->methods())
        {
            if (other_method->getNameInfo().getAsString() != our_method_decl->getNameInfo().getAsString()) {
                continue;
            }

            if (other_method->param_size() != our_method_decl->param_size()) {
                continue;
            }

            if (other_method->getCanonicalDecl() == our_method_decl->getCanonicalDecl()) {
                continue;
            }


            auto our_method_param = our_method_decl->param_begin();
            auto other_method_param = other_method->param_begin();
            size_t param_num = 0;

            RefStringType ref_type = RefStringType::None;

            for (; our_method_param != our_method_decl->param_end(); our_method_param++, other_method_param++, param_num++) {

                if (param_num == static_cast<size_t>(arg_num)) {
                    const auto other_param_type = (*other_method_param)->getType().getAsString();

                    const auto [other_param_is_string_ref_type, ref_string_type] = is_string_ref_parameter((*other_method_param)->getType(), getContext());

                    if (!other_param_is_string_ref_type) {
                        break;
                    }

                    if ((*our_method_param)->getType().getAsString() != "const char *") {
                        break;
                    }

                    ref_type = ref_string_type;

                    continue;
                }

                if ((*our_method_param)->getType() != (*other_method_param)->getType()) {
                    break;
                }
            }

            if (param_num == our_method_decl->param_size() &&
                    ref_type != RefStringType::None) {
                //llvm::outs() << "can be replaced\n";

                report(arg_member_call->getExprLoc(), m_replace_method_id)
                    .AddValue(ref_type == RefStringType::StringRef
                            ? "types::StringRef"
                            : "std::string_view");

                report(other_method->getLocation(), m_note_id);

                break;
            }
        }
    }

    return true;
}
