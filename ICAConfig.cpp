#include "ICAConfig.h"
#include "ICACommon.h"

#include <cstdlib>

ICAConfig::ICAConfig()
{
    static constexpr const char * no_url_env = "ICA_NO_URL";

    if (std::getenv(no_url_env) != nullptr) {
        m_use_url = false;
    }
}

std::optional<std::string> ICAConfig::parse(const std::vector<std::string> & args)
{
    const std::string_view checks_prefix = "checks=";
    const std::string_view no_url = "no-url";

    for (const auto & arg : args) {
        if (arg == no_url) {
            m_use_url = false;
            continue;
        }

        if (const auto [starts_with, check_list] = removePrefix(arg, checks_prefix); starts_with) {
            if (auto error = m_checks.parse(check_list); error) {
                return error;
            }
        }
    }

    return std::nullopt;
}
