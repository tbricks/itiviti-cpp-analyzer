#pragma once

#include "llvm/ADT/StringRef.h"

#include "boost/compressed_pair.hpp"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/SourceManager.h"

#include <algorithm>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <cstring>
#include <string>
#include <string_view>

template <class Node>
inline bool isExpansionInSystemHeader(Node * node, const clang::SourceManager & source_manager)
{
    const auto expansion_loc = source_manager.getExpansionLoc(node->getBeginLoc());
    if (expansion_loc.isInvalid()) {
        return false;
    }
    return source_manager.isInSystemHeader(expansion_loc);
}

template <class Node>
inline bool isMacroLocation(Node * node)
{
    const auto loc = node->getBeginLoc();
    return loc.isMacroID();
}

template <class Node>
inline bool isInvalidLocation(Node * node)
{
    const auto loc = node->getBeginLoc();
    return loc.isInvalid();
}

template <class Node>
inline bool isNolintLocation(Node * node, const clang::SourceManager & source_manager)
{
    const auto loc = node->getBeginLoc();
    bool error = false;
    const char * data  = source_manager.getCharacterData(loc, &error);
    if (error) {
        return false;
    }

    const char * line_break = std::strchr(data, '\n');
    const auto line = line_break ?
        llvm::StringRef{data, static_cast<std::size_t>(line_break - data)} :
        llvm::StringRef{data};

    size_t double_slash_idx = line.find("//");
    size_t nolint_idx = line.find("NOLINT");
    return double_slash_idx < nolint_idx && nolint_idx != llvm::StringRef::npos;
}

inline bool shouldProcessStmt(const clang::Stmt * stmt, const clang::SourceManager & source_manager)
{
    return    stmt
           && !isExpansionInSystemHeader(stmt, source_manager)
           && !isInvalidLocation(stmt)
           && !isMacroLocation(stmt)
           && !isNolintLocation(stmt, source_manager);
}

inline bool shouldProcessDecl(const clang::Decl * decl, const clang::SourceManager & source_manager)
{
    return    decl
           && !isExpansionInSystemHeader(decl, source_manager)
           && !isInvalidLocation(decl)
           && !isMacroLocation(decl)
           && !isNolintLocation(decl, source_manager);
}

inline bool shouldProcessExpr(const clang::Expr * expr, const clang::SourceManager & source_manager)
{
    return    expr
           && !isExpansionInSystemHeader(expr, source_manager)
           && !isInvalidLocation(expr)
           && !isMacroLocation(expr)
           && !isNolintLocation(expr, source_manager);
}

inline std::string_view sourceRangeAsString(const clang::SourceRange & range, const clang::SourceManager & source_manager)
{
    const auto * start = source_manager.getCharacterData(range.getBegin());
    const auto * end   = source_manager.getCharacterData(range.getEnd());
    return std::string_view(start, end - start + 1);
}

inline bool startsWith(const std::string_view str, const std::string_view prefix)
{
    if (str.length() < prefix.length()) {
        return false;
    }
    return std::equal(prefix.begin(), prefix.end(), str.begin());
}

inline std::pair<bool, std::string_view> removePrefix(std::string_view str, const std::string_view prefix)
{
    const bool starts_with = startsWith(str, prefix);
    if (starts_with) {
        str.remove_prefix(prefix.length());
    }
    return {starts_with, str};
}

inline std::string removeAllMatches(std::string str, const std::string_view substr)
{
    auto pos = std::string::npos;
    while ((pos = str.find(substr.data(), 0, substr.size())) != std::string::npos) {
        str.erase(str.begin() + pos, str.begin() + pos + substr.size());
    }
    return str;
}

inline std::string removeClassPrefix(std::string str)
{
    return removeAllMatches(removeAllMatches(std::move(str), "class "), "struct ");
}

inline std::string getUnqualifiedClassName(const clang::QualType & type)
{
    const auto t = type.getCanonicalType().getUnqualifiedType();
    return removeClassPrefix(t.getAsString());
}

// current version of libLLVM doesn't have a way to cast llvm::StringRef to std::string_view, but implicitly casts to std::string
inline std::string_view to_string_view(const llvm::StringRef sr) noexcept
{
    return std::string_view(sr.data(), sr.size());
}

inline bool operator == (const llvm::StringRef l, const std::string_view r) noexcept
{
    return l.equals(llvm::StringRef{r.data(), r.size()});
}
inline bool operator != (const llvm::StringRef l, const std::string_view r) noexcept
{
    return !(l == r);
}
inline bool operator == (const std::string_view l, const llvm::StringRef r) noexcept
{
    return (r == l);
}
inline bool operator != (const std::string_view l, const llvm::StringRef r) noexcept
{
    return !(r == l);
}

std::string wrapCheckNameWithURL(std::string_view check_name);

bool isIdenticalStmt(
        const clang::ASTContext & ctx,
        const clang::Stmt * stmt1,
        const clang::Stmt * stmt2,
        const bool ignore_side_effects);

const clang::DeclRefExpr * extractDeclRef(const clang::Expr * expr);

// iterators/references
struct RefTypeInfo
{
    bool is_ref = false;
    bool is_const = false;
    bool is_iter = false;
};
RefTypeInfo asRefType(const clang::QualType & type);

RefTypeInfo isIterator(const clang::CXXRecordDecl * decl);


// memorizing function
template <class F>
struct MemberFunctionTraits;

template <class In, class Out, class F>
struct MemberFunctionTraits<Out (F::*) (In)>
{
    using out = Out;
    using in = In;
};

template <class In, class Out, class F>
struct MemberFunctionTraits<Out (F::*) (In) const>
    : public MemberFunctionTraits<Out (F::*) (In)>
{
};

template <class L>
struct LambdaTraits
    : public MemberFunctionTraits<decltype(&L::operator())>
{
};

template <class Out, class In>
struct LambdaTraits<Out (*)(In)>
{
    using out = Out;
    using in = In;
};

/// Functor wrapper for memorizing results of unary functions for lazy calculations
///
/// \param F is unary function F is U(V v) - std::unordered_map<U,V> should be compilable with these arguments
///
template <class F, class Out = typename LambdaTraits<F>::out, class In = typename LambdaTraits<F>::in>
class MemorizingFunctor
{
public:
    MemorizingFunctor(F && f)
        : m_pair_functor_map(std::move(f))
    {
    }

    Out operator()(In key) const
    {
        auto [it, emplaced] = cache().try_emplace(key);

        if (emplaced) {
            it->second = functor()(key);
        }

        return it->second;
    }

private:
    const auto & functor() const
    {
        return m_pair_functor_map.first();
    }

    auto & cache() const
    {
        return m_pair_functor_map.second();
    }
private:
    mutable boost::compressed_pair<F, std::unordered_map<In, Out>> m_pair_functor_map;
};
