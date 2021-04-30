#pragma once

#include "ICAChecks.h"

#include <optional>
#include <string>
#include <vector>

class ICAConfig
{
public:
    ICAConfig();

    std::optional<std::string> parse(const std::vector<std::string> & args);

    bool get_use_url() const
    { return m_use_url; }

    const ICAChecks & get_checks() const
    { return m_checks; }

private:
    ICAChecks m_checks;
    bool m_use_url = true;
};
