#include <string>
#include <vector>
#include <string_view>

namespace tbricks::types {

struct StringRef
{
    std::string to_string() const { return {}; }
};

struct VIID
{
    int set_ref(const char * key, const char * value) { return 0; }
    int set_ref(const std::vector<StringRef> & key, const char * value) { return 0; }
    int set_ref(const std::string_view key, const char * value) { return 0; } // expected-note 2 {{overload defined here}}

    int set_view(const char * key, const char * value) { return 0; }
    int set_view(const StringRef * key, const char * value) { return 0; }
    int set_view(const StringRef key, const char * value); // expected-note 2 {{overload defined here}}

    void single_overload(const std::string_view key, const char * value) { }
};

inline int VIID::set_view(StringRef key, const char * value) { return 0; }

struct DateTime {};

struct DateTimeFactory
{
    DateTime create_from_string(const char * local_date) const { return {}; }
    DateTime create_from_string(const StringRef local_date) const
    { return create_from_string(local_date.to_string().c_str()); }
};

}

std::string get_string() { return "qweqwe"; }

int main()
{
    std::string s;
    tbricks::types::VIID viid;
    viid.set_ref(s.c_str(), "qweqwe"); // expected-warning {{call to c_str can be removed, overload with std::string_view available}}
    viid.set_ref("qwe", "qweqwe");
    viid.set_ref(get_string().c_str(), "qweqwe"); // expected-warning {{call to c_str can be removed, overload with std::string_view available}}

    viid.set_view(s.c_str(), "qweqwe"); // expected-warning {{call to c_str can be removed, overload with types::StringRef available}}
    viid.set_view("qwe", "qweqwe");
    viid.set_view(get_string().c_str(), "qweqwe"); // expected-warning {{call to c_str can be removed, overload with types::StringRef available}}

    viid.single_overload(s.c_str(), "qwe"); // expected-warning {{call to c_str can be removed, implicit cast from std::string is possible}}
    viid.single_overload(std::string("test").c_str(), "qwe"); // expected-warning {{call to c_str can be removed, implicit cast from std::string is possible}}

    tbricks::types::DateTimeFactory f;
    f.create_from_string(s.c_str());
}
