#include "CCIteratorVisitor.h"
#include "ICACommon.h"
#include "ICAConfig.h"

#include <algorithm>
#include <boost/bimap/bimap.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclFriend.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/OperatorKinds.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Frontend/CompilerInstance.h>

#include <iterator>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string_view>

CCIteratorVisitor::CCIteratorVisitor(clang::CompilerInstance & ci, const ICAConfig & checks)
    : ICAVisitor(ci, checks)
{
    if (!isEnabled()) {
        return;
    }
    m_warn_id = getCustomDiagID(correct_iterator, "%0 declared as iterator in %1, but it doesn't complete iterator contract:\n%2");
    m_note_id = getCustomDiagID(clang::DiagnosticIDs::Note, "declared as 'iterator' alias here");
}

namespace {

    struct Iterator
    {   enum IteratorType
        { None, Traited, Forward, Bidirectional, Random };

        Iterator(const clang::CXXRecordDecl * fwd_iter_decl)
            : self(fwd_iter_decl)
        {
        }
        virtual ~Iterator() = default;

        void checkTypedef(const clang::TypedefNameDecl *alias);
        std::string getUndeclaredTypes() const;
        IteratorType getKind() const;

        std::unique_ptr<Iterator> getCurrentIteratorType() const;

        const clang::CXXRecordDecl * self;
        clang::QualType value_type;
        clang::QualType reference;
        clang::QualType pointer;
        clang::QualType difference_type;
        clang::QualType iterator_category;

        IteratorType type = None;
        bool modified = false;
    };

    void Iterator::checkTypedef(const clang::TypedefNameDecl *alias)
    {
        if (!alias) {
            return;
        }
        auto identifier = alias->getIdentifier();
#define CHECK_TYPENAME(checked_typename) \
else if (identifier->isStr(#checked_typename)) { \
    checked_typename = alias->getUnderlyingType().getCanonicalType(); \
    modified = true; \
}
        if (!identifier) {
            return;
        }
        CHECK_TYPENAME(value_type)
        CHECK_TYPENAME(reference)
        CHECK_TYPENAME(pointer)
        CHECK_TYPENAME(difference_type)
        CHECK_TYPENAME(iterator_category)
#undef CHECK_TYPENAME
    }

    std::string Iterator::getUndeclaredTypes() const
    {
        clang::QualType empty;
        std::string res;
        auto append = [&res](const char * to_append) -> std::string & {
            if (!res.empty()) {
                res += ", ";
            }
            return res += to_append;
        };

#define APPEND(to_append) if (to_append == empty) append("'" #to_append "'")
        APPEND(value_type);
        APPEND(reference);
        APPEND(pointer);
        APPEND(difference_type);
        APPEND(iterator_category);
#undef APPEND

        return res;
    }

    auto Iterator::getKind() const -> IteratorType
    {
        if (!modified) {
            return type;
        }
        clang::QualType empty;
        if (value_type == empty || reference == empty || pointer == empty || difference_type == empty || iterator_category == empty) {
            return IteratorType::None;
        }

        auto iter_type_str = removeClassPrefix(iterator_category.getAsString());
        if (iter_type_str == "std::forward_iterator_tag") {
            return IteratorType::Forward;
        }
        if (iter_type_str == "std::bidirectional_iterator_tag") {
            return IteratorType::Bidirectional;
        }
        if (iter_type_str == "std::random_access_iterator_tag") {
            return IteratorType::Random;
        }
        return IteratorType::Traited;
    }


    struct ForwardIterator : Iterator
    {
        enum CheckIncDecResult
        { None, Pre, Post };

        ForwardIterator(const Iterator & base)
            : Iterator(base)
            , copy_constructible(self->hasSimpleCopyConstructor())
            , copy_assignable(self->hasCopyAssignmentWithConstParam())
            , destructible(self->hasSimpleDestructor())
        {
            type = Forward;
        }

        virtual ~ForwardIterator() = default;

        void checkNestedFunc(const clang::FunctionDecl * func);
        void checkTypedef(const clang::TypedefNameDecl * alias);

        virtual bool checkOverloadedOperator(const clang::FunctionDecl * oper);
        CheckIncDecResult checkIncDec(const clang::CXXMethodDecl * oper);

        std::string getUndeclaredTypes();
        IteratorType getIteratorType();

        static bool checkComparison(const clang::FunctionDecl * comp);
        static bool isConstLvalueParm(const clang::ParmVarDecl * parm);
        static bool classof(const Iterator * iter);


        bool copy_constructible = false;
        bool copy_assignable = false;
        bool destructible = false;

        bool eq_comparable = false;
        bool ne_comparabe = false;

        bool dereferencable = false;
        bool pointable = false;
        bool pre_incrementable = false;
        bool post_incrementable = false;
    };

    void ForwardIterator::checkNestedFunc(const clang::FunctionDecl * func)
    {
        if (!func) {
            return;
        }
        if (func->getAccess() != clang::AS_public || func->isDeleted()) {
            return;
        }

        if (auto ctor = clang::dyn_cast<clang::CXXConstructorDecl>(func); ctor && ctor->isCopyConstructor()) {
            copy_constructible = true;
            return;
        } else if (auto method = clang::dyn_cast<clang::CXXMethodDecl>(func);
                   method && method->isCopyAssignmentOperator()) {
                copy_assignable = true;
        } else if (clang::isa<clang::CXXDestructorDecl>(func)) {
            destructible = true;
        } else if (func->isOverloadedOperator()) {
            checkOverloadedOperator(func);
        }
    }

    bool ForwardIterator::checkOverloadedOperator(const clang::FunctionDecl *oper)
    {
        auto op_kind = oper->getOverloadedOperator();
        switch (op_kind) {
        case clang::OO_EqualEqual:
            return eq_comparable = checkComparison(oper);
        case clang::OO_ExclaimEqual:
            return ne_comparabe = checkComparison(oper);
        case clang::OO_PlusPlus:
            if (auto inc_dec_res = checkIncDec(clang::dyn_cast<clang::CXXMethodDecl>(oper));
                inc_dec_res == CheckIncDecResult::Pre) {
                return pre_incrementable = true;
            } else if (inc_dec_res == CheckIncDecResult::Post) {
                return post_incrementable = true;
            } else {
                return false;
            }
        case clang::OO_Star:
            return dereferencable = oper->getReturnType().getCanonicalType() == reference;
        case clang::OO_Arrow:
            return pointable = oper->getReturnType().getCanonicalType() == pointer;
        default:
            return false;
        }
    }

    auto ForwardIterator::checkIncDec(const clang::CXXMethodDecl *oper) -> CheckIncDecResult
    {
        if (!oper) {
            return CheckIncDecResult::None;
        }
        if (oper->isConst()) {
            return CheckIncDecResult::None;
        }

        auto ret_type = oper->getReturnType();
        auto ret_type_decl = ret_type.getNonReferenceType()->getAsCXXRecordDecl();
        if (oper->getNumParams() == 0) {
            if (ret_type->isReferenceType() && ret_type_decl == self) {
                return CheckIncDecResult::Pre;
            }
        } else if (oper->getNumParams() == 1) {
            if (!ret_type->isReferenceType() && ret_type_decl == self) {
                return CheckIncDecResult::Post;
            }
        }
        return CheckIncDecResult::None;
    }

    bool ForwardIterator::checkComparison(const clang::FunctionDecl * comp)
    {
        const bool returns_bool = comp->getReturnType()->isBooleanType();
        bool const_params = std::all_of(comp->param_begin(), comp->param_end(), isConstLvalueParm);
        if (comp->getNumParams() == 1) {
            auto as_method = clang::dyn_cast<clang::CXXMethodDecl>(comp);
            const_params &= as_method && as_method->isConst();
        } else if (comp->getNumParams() != 2) {
            return false;
        }
        return returns_bool && const_params;
    }

    bool ForwardIterator::isConstLvalueParm(const clang::ParmVarDecl * parm)
    {
        if (!parm) {
            return false;
        }
        auto type = parm->getType();
        return type->isLValueReferenceType() && type.getNonReferenceType().isConstQualified();
    }

    bool ForwardIterator::classof(const Iterator *iter)
    {
        return iter->getKind() >= Forward;
    }


    struct BidirectionalIterator : ForwardIterator
    {
        BidirectionalIterator(const Iterator & base)
            : ForwardIterator(base)
        {
            type = Bidirectional;
        }

        virtual ~BidirectionalIterator() = default;

        bool pre_decrementable = false;
        bool post_decrementable = false;

        bool checkOverloadedOperator(const clang::FunctionDecl * oper) override;

        static bool classof(const Iterator * iter);
    };

    bool BidirectionalIterator::checkOverloadedOperator(const clang::FunctionDecl *oper)
    {
        if (ForwardIterator::checkOverloadedOperator(oper)) {
            return true;
        }
        auto op_kind = oper->getOverloadedOperator();
        if (op_kind == clang::OO_MinusMinus) {
            auto inc_dec_res = checkIncDec(clang::dyn_cast<clang::CXXMethodDecl>(oper));
            if (inc_dec_res == CheckIncDecResult::Pre) {
                return pre_decrementable = true;
            } else if (inc_dec_res == CheckIncDecResult::Post) {
                return post_decrementable = true;
            }
        }
        return false;
    }


    std::unique_ptr<Iterator> Iterator::getCurrentIteratorType() const
    {
        switch (getKind()) {
        case IteratorType::None:
            return nullptr;
        case Iterator::Traited:
            return std::make_unique<Iterator>(*this);
        case IteratorType::Forward:
            return std::make_unique<ForwardIterator>(*this);
        case IteratorType::Bidirectional:
        case IteratorType::Random:
            return std::make_unique<BidirectionalIterator>(*this);
        }
    }

    bool BidirectionalIterator::classof(const Iterator *iter)
    {
        return iter->getKind() >= Bidirectional;
    }

}

std::unique_ptr<std::stringstream> CCIteratorVisitor::checkIterator(const clang::CXXRecordDecl * record_decl)
{
    if (!record_decl) {
        return nullptr;
    }

    struct {
        std::unique_ptr<std::stringstream> strm;
        std::ostream & operator () ()
        {
            if (!strm) {
                strm = std::make_unique<std::stringstream>();
            }
            return *strm;
        }
    } errs;

    if (auto inst_decl = clang::dyn_cast<clang::ClassTemplateSpecializationDecl>(record_decl)) {
        record_decl = inst_decl->getSpecializedTemplate()->getTemplatedDecl();
    }

    Iterator checked_decl(record_decl);

    std::for_each(checked_decl.self->decls_begin(), checked_decl.self->decls_end(), [&checked_decl](auto decl) {
        checked_decl.checkTypedef(clang::dyn_cast<clang::TypedefNameDecl>(decl));
    });

    auto iter_unpecified = checked_decl.getCurrentIteratorType();
    auto fwd_iter = clang::dyn_cast_or_null<ForwardIterator>(iter_unpecified.get());

    if (!fwd_iter) {
        std::string undeclared = checked_decl.getUndeclaredTypes();
        errs() << undeclared << " should be declared in an iterator type\n";
        return std::move(errs.strm);
    }


    for (const clang::Decl * decl : fwd_iter->self->decls()) {
        auto func = clang::dyn_cast<clang::FunctionDecl>(decl);
        if (!func) {
            auto friend_decl = clang::dyn_cast<clang::FriendDecl>(decl);
            if (friend_decl) {
                func = clang::dyn_cast<clang::FunctionDecl>(friend_decl->getFriendDecl());
            }
        }
        fwd_iter->checkNestedFunc(func);
    }

    std::string_view iter_name = to_string_view(record_decl->getName());

    auto oo_expected = [&errs]() -> std::ostream & {
        return errs() << "expected operator overload: ";
    };
    auto copy_ctor_expected = [&errs]() -> std::ostream & {
        return errs() << "expected copy ctor: ";
    };
    auto dtor_expected = [&errs]() -> std::ostream & {
        return errs() << "expected dtor: ";
    };

    if (!fwd_iter->copy_constructible) {
        copy_ctor_expected() << iter_name << "(const " << iter_name << " &);\n";
    }
    if (!fwd_iter->copy_assignable) {
        oo_expected() << iter_name << " & operator = (const " << iter_name << " &);" << '\n';
    }
    if (!fwd_iter->destructible) {
        dtor_expected() << '~' << iter_name << '\n';
    }
    if (!fwd_iter->eq_comparable) {
        oo_expected() << "bool operator == (const " << iter_name << "&) const;" << '\n';
    }
    if (!fwd_iter->ne_comparabe) {
        oo_expected() << "bool operator != (const " << iter_name << "&) const;" << '\n';
    }
    if (!fwd_iter->dereferencable) {
        oo_expected() << "reference operator * ();" << '\n';
    }
    if (!fwd_iter->pointable) {
        oo_expected() << "pointer operator -> ();" << '\n';
    }
    if (!fwd_iter->pre_incrementable) {
        oo_expected() << iter_name << " & operator ++ ();" << '\n';
    }
    if (!fwd_iter->post_incrementable) {
        oo_expected() << iter_name << " operator ++ (int)" << '\n';
    }

    auto bidir_iter = clang::dyn_cast<BidirectionalIterator>(fwd_iter);
    if (!bidir_iter) {
        return std::move(errs.strm);
    }

    if (!bidir_iter->pre_decrementable) {
        oo_expected() << iter_name << " & operator -- ();" << '\n';
    }
    if (!bidir_iter->post_decrementable) {
        oo_expected() << iter_name << " operator -- (int);" << '\n';
    }
    return std::move(errs.strm);
}

bool CCIteratorVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl *record_decl)
{
    if (!shouldProcessDecl(record_decl, getSM())) {
        return true;
    }
    auto check_class_and_report = [this, iter_owner(record_decl)](const clang::CXXRecordDecl * iter_class, const clang::NamedDecl * iterator_name) {
        if (!iter_class) {
            return;
        }
        if (auto errs = checkIterator(iter_class)) {
            report(iter_class->getLocation(), m_warn_id)
                .AddValue(iter_class)
                .AddValue(iter_owner)
                .AddValue(errs->str());
            report(iterator_name->getLocation(), m_note_id);
        }
    };

    auto is_iterator_name = [](const clang::NamedDecl * decl) {
        if (!decl) {
            return false;
        }
        auto identifier = decl->getIdentifier();
        return identifier && identifier->isStr("iterator");
    };
    if (record_decl->decls_empty()) {
        return true;
    }
    std::for_each(++(record_decl->decls_begin()), record_decl->decls_end(), [&](const clang::Decl * decl) {
        if (auto alias = clang::dyn_cast<clang::TypedefNameDecl>(decl); is_iterator_name(alias)) {
            auto underlying_type = alias->getUnderlyingType();
            auto iter_decl = underlying_type->getAsCXXRecordDecl();
            if (!iter_decl) {
                if (auto template_spec = underlying_type->getAs<clang::TemplateSpecializationType>()) {
                    auto template_decl = template_spec->getTemplateName().getAsTemplateDecl();
                    iter_decl = clang::dyn_cast<clang::CXXRecordDecl>(template_decl->getTemplatedDecl());
                }
            }
            check_class_and_report(iter_decl, alias);
        } else if (auto iter_decl = clang::dyn_cast<clang::CXXRecordDecl>(decl); is_iterator_name(iter_decl)) {
            check_class_and_report(iter_decl, iter_decl);
        }
    });
    return true;
}