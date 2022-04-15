#pragma once

#include "clang/Basic/Diagnostic.h"

#include <iosfwd>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>

namespace ica {

class Check
{
public:
    enum State
    {
        Disabled, Note, Warning, Error,
    };

public:
    constexpr Check() noexcept = default;
    constexpr Check(const State state) noexcept : m_state(state)
    { }

    Check & operator = (const State state) noexcept
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

    friend std::ostream & operator << (std::ostream & strm, const Check & check)
    {
        return strm << std::string(to_string(check.m_state));
    }

private:
    State m_state = Disabled;
};

class Checks;
std::ostream & operator << (std::ostream & strm, const Checks & checks);

class Checks
{
    friend std::ostream & operator << (std::ostream & strm, const Checks & checks);

public:
    Checks() = default;
    Checks(const Checks & from) = default;

    explicit Checks(const std::string & checks)
    {
        parse(checks);
    }

    void clear()
    {
        m_checks.clear();
        m_all = Check::Disabled;
    }

    std::optional<std::string> parse(std::string_view checks);

    const Check operator [] (std::string_view check) const noexcept;

private:
    Check & get(std::string_view check) noexcept;

private:
    Check m_all = Check::Disabled;
    std::unordered_map<std::string_view, Check> m_checks;
};

} // namespace ica
