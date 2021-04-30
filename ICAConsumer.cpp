#include "ICAConsumer.h"
#include "ICACommon.h"

ICAConsumer::ICAConsumer(clang::CompilerInstance & ci, ICAConfig config) :
    m_config(std::move(config)),
    m_translation_unit_visitor(ci, m_config),
    m_top_level_decl_visitor(ci, m_config)
{
}

void ICAConsumer::HandleTranslationUnit(clang::ASTContext & context)
{
    if (m_translation_unit_visitor.isEnabled()) {
        m_translation_unit_visitor.setContext(context);
        m_translation_unit_visitor.TraverseDecl(context.getTranslationUnitDecl());
        m_translation_unit_visitor.printDiagnostic(context);
    }

}

bool ICAConsumer::HandleTopLevelDecl(clang::DeclGroupRef decl_group)
{
    for (const auto decl : decl_group) {
        auto & context = decl->getASTContext();
        auto & source_manager = context.getSourceManager();

        if (shouldProcessDecl(decl, source_manager)) {
            if (m_top_level_decl_visitor.isEnabled()) {
                m_top_level_decl_visitor.clear();
                m_top_level_decl_visitor.setContext(context);
                m_top_level_decl_visitor.TraverseDecl(decl);
                m_top_level_decl_visitor.printDiagnostic(context);
            }
        }
    }

    return true;
}
