#pragma once

#include "shared/common/Checks.h"

#include <optional>
#include <string>
#include <vector>

namespace ica {

class Config
{
public:
    Config();

    std::optional<std::string> parse(const std::vector<std::string> & args);

    bool get_use_url() const
    { return m_use_url; }

    const Checks & get_checks() const
    { return m_checks; }

private:
    Checks m_checks;
    bool m_use_url = true;
};

} // namespace ica
