#pragma once

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"

#include "internal/checks/ExclusiveUnitedVisitors.h"

#include "shared/common/Config.h"
#include "shared/common/UnitedVisitor.h"

#include "shared/checks/CTypeCharVisitor.h"
#include "shared/checks/EmplaceDefaultValueVisitor.h"
#include "shared/checks/EraseInLoopVisitor.h"
#include "shared/checks/FindEmplaceVisitor.h"
#include "shared/checks/InlineMethodsInClassBodyVisitor.h"
#include "shared/checks/LockGuardReleaseVisitor.h"
#include "shared/checks/NoexceptVisitor.h"
#include "shared/checks/MoveStringStreamVisitor.h"
#include "shared/checks/RemoveCStrVisitor.h"

namespace ica {

class Consumer : public clang::ASTConsumer
{

    using TranslationUnitUV = UnitedVisitor<
        ExclusiveTranslationUnitVisitor,
        CTypeCharVisitor,
        EmplaceDefaultValueVisitor,
        EraseInLoopVisitor,
        InlineMethodsInClassBodyVisitor,
        LockGuardReleaseVisitor,
        NoexceptVisitor,
        RemoveCStrVisitor>;
    using TopLevelDeclUV =  UnitedVisitor<
        ExclusiveTopLevelDeclVisitor,
        FindEmplaceVisitor,
        MoveStringStreamVisitor>;

public:
    explicit Consumer(clang::CompilerInstance & ci, Config config);

    virtual void HandleTranslationUnit(clang::ASTContext & context) override;
    virtual bool HandleTopLevelDecl(clang::DeclGroupRef decl_group) override;

private:
    Config m_config;

    TranslationUnitUV m_translation_unit_visitor;

    TopLevelDeclUV m_top_level_decl_visitor;
};

} // namespace ica
