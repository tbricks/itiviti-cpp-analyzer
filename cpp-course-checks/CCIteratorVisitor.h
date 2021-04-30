#pragma once

#include "ICAConfig.h"
#include "ICADiagnosticsBuilder.h"
#include "ICAVisitor.h"


#include <boost/bimap/bimap.hpp>
#include <boost/multi_index_container.hpp>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <clang/Frontend/CompilerInstance.h>

#include <array>
#include <iterator>
#include <sstream>
#include <string_view>
#include <unordered_map>

class CCIteratorVisitor : public ICAVisitor<CCIteratorVisitor>
{
    static constexpr auto * correct_iterator = "correct-iterator";

public:
    static constexpr auto check_names = make_check_names(correct_iterator);

public:
    CCIteratorVisitor(clang::CompilerInstance & ci, const ICAConfig & checks);

    bool VisitCXXRecordDecl(clang::CXXRecordDecl * record_decl);

    void printDiagnostic(clang::ASTContext &) {}
    void clear() {}

    std::unique_ptr<std::stringstream> checkIterator(const clang::CXXRecordDecl * record_decl);

private:
    DiagnosticID m_warn_id;
    DiagnosticID m_note_id;
};