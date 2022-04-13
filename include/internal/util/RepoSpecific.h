#include <string>
#include <string_view>

inline std::string getCheckURL(const std::string_view check_name)
{
    return "https://github.com/tbricks/itiviti-cpp-analyzer/blob/main/Checks.md#"
        + std::string(check_name);
}
