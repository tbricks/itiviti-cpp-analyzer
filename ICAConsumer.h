#pragma once

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"

#include "ICAConfig.h"
#include "ICAUnitedVisitor.h"

#include "ICACTypeCharVisitor.h"
#include "ICAEmplaceDefaultValueVisitor.h"
#include "ICAEraseInLoopVisitor.h"
#include "ICAFindEmplaceVisitor.h"
#include "ICANoexceptVisitor.h"
#include "ICARemoveCStrVisitor.h"

#include "CCBadRandVisitor.h"
#include "CCIteratorVisitor.h"
#include "CCForRangeConstVisitor.h"
#include "CCImproperMoveVisitor.h"
#include "CCReturnValueVisitor.h"
#include "CCMiscellaneousVisitor.h"
#include "CCInitMembersVisitor.h"

class ICAConsumer : public clang::ASTConsumer
{
    using TranslationUnitUV = ICAUnitedVisitor<
        ICACTypeCharVisitor,
        ICAEmplaceDefaultValueVisitor,
        ICAEraseInLoopVisitor,
        ICANoexceptVisitor,
        ICARemoveCStrVisitor>;
    using TopLevelDeclUV =  ICAUnitedVisitor<
        ICAFindEmplaceVisitor,
        CCInitMembersVisitor,
        CCForRangeConstVisitor,
        CCImproperMoveVisitor,
        CCIteratorVisitor,
        CCReturnValueVisitor,
        CCMiscellaneousVisitor,
        CCBadRandVisitor>;

public:
    explicit ICAConsumer(clang::CompilerInstance & ci, ICAConfig config);

    virtual void HandleTranslationUnit(clang::ASTContext & context) override;
    virtual bool HandleTopLevelDecl(clang::DeclGroupRef decl_group) override;

private:
    ICAConfig m_config;

    TranslationUnitUV m_translation_unit_visitor;

    TopLevelDeclUV m_top_level_decl_visitor;
};
