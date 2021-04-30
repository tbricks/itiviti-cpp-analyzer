#pragma once

#include "clang/Basic/Diagnostic.h"

#include <iosfwd>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>

class ICACheck
{
public:
    enum State
    {
        Disabled, Note, Warning, Error,
    };

public:
    constexpr ICACheck() noexcept = default;
    constexpr ICACheck(const State state) noexcept : m_state(state)
    { }

    ICACheck & operator = (const State state) noexcept
    { m_state = state; return *this; }

    constexpr explicit operator bool () const noexcept
    { return m_state != Disabled; }

    constexpr operator clang::DiagnosticIDs::Level () const noexcept
    {
        switch (m_state) {
        case Disabled: return clang::DiagnosticIDs::Ignored;
        case Error   : return clang::DiagnosticIDs::Error;
        case Note    : return clang::DiagnosticIDs::Note;
        case Warning : return clang::DiagnosticIDs::Warning;
        }

        __builtin_unreachable(); // just to silence -Wreturn-type warning
    }

    static constexpr std::string_view to_string(const State state) noexcept
    {
        switch (state) {
        case Disabled: return "none";
        case Error   : return  "err";
        case Note    : return "note";
        case Warning : return "warn";
        }

        __builtin_unreachable(); // just to silence -Wreturn-type warning
    }

    static std::optional<State> parse(const std::string_view sw) noexcept
    {
        if (sw == to_string(Disabled)) return Disabled;
        if (sw == to_string(Warning)) return Warning;
        if (sw == to_string(Error)) return Error;
        if (sw == to_string(Note)) return Note;
        return std::nullopt;
    }

    friend std::ostream & operator << (std::ostream & strm, const ICACheck & check)
    {
        return strm << std::string(to_string(check.m_state));
    }

private:
    State m_state = Disabled;
};

class ICAChecks;
std::ostream & operator << (std::ostream & strm, const ICAChecks & checks);

class ICAChecks
{
    friend std::ostream & operator << (std::ostream & strm, const ICAChecks & checks);

public:
    ICAChecks() = default;

    explicit ICAChecks(const std::string & checks)
    {
        parse(checks);
    }

    void clear()
    {
        m_checks.clear();
        m_checks_str.clear();
        m_all = ICACheck::Disabled;
    }

    std::optional<std::string> parse(std::string_view checks);

    const ICACheck operator [] (std::string_view check) const noexcept;

private:
    ICACheck & get(std::string_view check) noexcept;

private:
    ICACheck m_all;
    std::string m_checks_str;
    std::unordered_map<std::string_view, ICACheck> m_checks;
};
