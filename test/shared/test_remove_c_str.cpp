#include <string>
#include <vector>
#include <string_view>

struct Container
{
    int set_ref(const char * key, const char * value) { return 0; }
    int set_ref(const std::vector<std::string_view> & key, const char * value) { return 0; }
    int set_ref(const std::string_view key, const char * value) { return 0; } // expected-note 2 {{overload defined here}}

    int set_view(const char * key, const char * value) { return 0; }
    int set_view(const std::string_view * key, const char * value) { return 0; }
    int set_view(const std::string_view key, const char * value); // expected-note 2 {{overload defined here}}

    void single_overload(const std::string_view key, const char * value) { }
};


std::string get_string() { return "qweqwe"; }

struct RecursionExample
{
    RecursionExample(const char * const_char) {}
    RecursionExample(std::string & str) { RecursionExample(str.c_str()); }
};

int main()
{
    std::string s;
    Container c;
    c.set_ref(s.c_str(), "qweqwe"); // expected-warning {{call to c_str can be removed, overload with std::string_view available}}
    c.set_ref("qwe", "qweqwe");
    c.set_ref(get_string().c_str(), "qweqwe"); // expected-warning {{call to c_str can be removed, overload with std::string_view available}}

    c.set_view(s.c_str(), "qweqwe"); // expected-warning {{call to c_str can be removed, overload with std::string_view available}}
    c.set_view("qwe", "qweqwe");
    c.set_view(get_string().c_str(), "qweqwe"); // expected-warning {{call to c_str can be removed, overload with std::string_view available}}

    c.single_overload(s.c_str(), "qwe"); // expected-warning {{call to c_str can be removed, implicit cast from std::string is possible}}
    c.single_overload(std::string("test").c_str(), "qwe"); // expected-warning {{call to c_str can be removed, implicit cast from std::string is possible}}
}
