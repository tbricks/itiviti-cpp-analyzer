#include <algorithm>
#include <string>
#include <utility>
#include <memory>

std::string fst_half(const std::string s)
{
    return s.substr(0, s.length() / 2);
}

void move_rvalue()
{
    std::string s(std::move(fst_half("42")));       // expected-warning {{'std::move' argument is already an rvalue, no need to move it}}
    std::string s1(std::move(std::string{"lul"}));  // expected-warning {{'std::move' argument is already an rvalue, no need to move it}}
}

void move_lvalue(std::string & s)
{
    std::string s1(std::move(s)); // expected-warning {{'s' got as non-const lvalue, moving from it may invalidate it's data}}
}

template <class T>
struct TemplateStruct
{
    void method(T & t)
    {
        my_unsafe(t);       // no warning
        val = std::move(t); // expected-warning 1+ {{'t' got as non-const lvalue, moving from it may invalidate it's data}}
    }

private:
    auto my_unsafe(T & t)
    {
        return std::move(t);
    }

    T val;
};

void consume_rvalue(std::string &&);

const std::string & get_s1(std::string * s)
{
    return std::move(*s); // expected-warning {{result of 'std::move' is implicitly cast to const reference}}
}

const std::string & get_s2(std::string * s)
{
    return *std::move(s);   // clang-tidy: performance-move-const-arg
                            // expected-warning@-1 {{result of 'std::move' is implicitly cast to const reference}}
}

const char & get_c1(std::string s)
{
    return std::move(*s.cbegin()); // clang-tidy: performance-move-const-arg
}

const char & get_c2(std::string s)
{
    return *std::move(s.cbegin());  // expected-warning {{'std::move' argument is already an rvalue, no need to move it}}
                                    // expected-warning@-1 {{result of 'std::move' is implicitly cast to const reference}}
}

template <class T>
auto try_move(T && value, int i)
{
    return std::move(value); // expected-warning {{'value' got as non-const lvalue, moving from it may invalidate it's data}}
}

template <class... Args>
void move_all(std::string * s, Args && ... args)
{
    std::move(s);               // clang-tidy: performance-move-const-arg
    (..., (std::move(args)));   // expected-warning 1+ {{'args' got as non-const lvalue, moving from it may invalidate it's data}}
}

void never_move_str(std::string && s) // expected-warning {{'s' is got as rvalue-reference, but never modified}}
{}

template <class T>
void never_move(T && t)
{}

struct StringPair
{
    std::string first, second;
    StringPair(StringPair && rhs)
        : first(std::move(rhs.first))
    {
        second = std::move(rhs.second);
    }

    StringPair(std::unique_ptr<StringPair> && ptr) // expected-warning {{'ptr' is got as rvalue-reference, but never modified}}
    {
        first = std::move(ptr->first);
        second = std::move(ptr->second);
    }
};

template <class Key, class Value>
struct MapEntry
{
    Key key;
    Value value;
    MapEntry() = default;

    MapEntry(MapEntry && rhs)
        : key(std::move(rhs.key))
        , value(std::move(rhs.value))
    {}
};

void instantiate()
{
    TemplateStruct<int> ts;
    int a = 3;
    ts.method(a);

    try_move(&a, 1);
    try_move(a, 1);
    move_all(new std::string{}, &a, a, ts);

    auto lambda = [](int a) {
        return 3;
    };

    std::string s;
    never_move_str(std::move(s));
    never_move(std::move(s));

    MapEntry<std::string, int> me1;
    decltype(me1) me2(std::move(me1));
}

void const_rvalue_param(const std::string && rhs) // expected-warning {{'rhs' is declared as const rvalue. Consider using either rvalue or const lvalue instead}}
{
}