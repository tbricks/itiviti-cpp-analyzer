#include "ICAChecks.h"

#include <cassert>
#include <iostream>
#include <variant>

namespace parser {

std::pair<std::string_view, std::string_view> split_by(
        const std::string_view str,
        const char delim,
        const std::string_view when_empty = {})
{
    std::string_view f(str), s(str);

    const auto pos = str.find(delim);
    if (pos == std::string::npos) {
        s = when_empty;
    } else {
        f.remove_suffix(str.size() - pos);
        s.remove_prefix(pos + 1);
    }

    return {f, s};
}

using Done = std::monostate;
using Error = std::string;
using Value = std::pair<std::string_view, ICACheck::State>;

using Variant = std::variant<Done, Error, Value>;

struct Result : Variant
{
    using Variant::Variant;

    bool is_value() const noexcept
    { return std::holds_alternative<Value>(*this); }

    bool is_error() const noexcept
    { return std::holds_alternative<Error>(*this); }

    bool is_done() const
    { return std::holds_alternative<Done>(*this); }

    decltype(auto) value() { return std::get<Value>(*this); }
    decltype(auto) error() { return std::get<Error>(*this); }
    decltype(auto) value() const { return std::get<Value>(*this); }
    decltype(auto) error() const { return std::get<Error>(*this); }

    decltype(auto) operator * () { return value(); }
    decltype(auto) operator * () const { return value(); }

    explicit operator bool () const noexcept
    { return is_value(); }
};


inline std::string operator + (std::string l, const std::string_view r)
{
    l.append(r.begin(), r.end());
    return l;
}


class Parser
{
    static constexpr inline auto warning = ICACheck::to_string(ICACheck::Warning);

public:
    Parser(const std::string & checks_str)
        : m_current_str(checks_str)
    { }

    Result operator () ()
    {
        using namespace std::literals::string_literals;

        if (m_current_str.empty()) {
            return Done{};
        }

        std::string_view curr;
        std::tie(curr, m_current_str) = split_by(m_current_str, ',');

        if (curr.empty()) {
            return (*this)();
        }

        // -check  ->  check=none
        if (curr[0] == '-') { // starts_with
            if (curr.find('=') != std::string::npos) {
                return Error{"Can't parse check '"s + curr + "': unexpected '='"};
            }
            curr.remove_prefix(1);
            return Value{curr, ICACheck::Disabled};
        }

        auto [check_name, check_state] = split_by(curr, '=', warning);
        if (const auto state = ICACheck::parse(check_state); !state) {
            return Error{"Can't parse check '"s + curr + "': unknown '" + check_state + "'"};
        } else {
            return Value{check_name, *state};
        }
    }

private:
    std::string_view m_current_str;
};

} // namespace parser


const ICACheck ICAChecks::operator [] (const std::string_view check) const noexcept
{
    if (const auto it = m_checks.find(check); it != m_checks.end()) {
        return it->second;
    } else {
        return m_all;
    }
}

ICACheck & ICAChecks::get(const std::string_view check) noexcept
{
    return (check == "all") ? m_all : m_checks[check];
}

std::optional<std::string> ICAChecks::parse(const std::string_view checks_list)
{
    clear();
    m_checks_str = checks_list;

    parser::Result result;
    parser::Parser parser(m_checks_str);

    while ((result = parser())) {
        const auto [check, state] = *result;
        get(check) = state;
    }

    if (result.is_error()) {
        return result.error();
    }

    return std::nullopt;
}

std::ostream & operator << (std::ostream & strm, const ICAChecks & checks)
{
    strm << "all = " << checks.m_all << '\n';
    for (const auto & check : checks.m_checks) {
        strm << check.first << " = " << check.second << '\n';
    }
    return strm;
}
