#include <cassert>
#include <chrono>
#include <cstring>
#include <exception>
#include <string>
#include <type_traits>
#include <utility>

void throwing_func();

struct SomeStruct
{
    SomeStruct() = default;

    SomeStruct(const SomeStruct &) = default;

    SomeStruct(const char *);

    SomeStruct & operator = (const SomeStruct &) = default;

    void throw_string(const char *);

    void method() noexcept;

    static void some_method();
};

void call_throwing_func() noexcept // expected-warning {{there is call of non-noexcept function within noexcept-specified}}
{
    throwing_func(); // expected-note {{non-noexcept function call is here}}
}

void call_throwing_static_method() noexcept // expected-warning {{there is call of non-noexcept function within noexcept-specified}}
{
    SomeStruct::some_method(); // expected-note {{non-noexcept function call is here}}
}

void call_throwing_ctor() noexcept // expected-warning {{there is call of non-noexcept function within noexcept-specified}}
{
    SomeStruct s("throwing ctor"); // expected-note {{non-noexcept function call is here}}
}

void call_throwing_method_template() noexcept // expected-warning {{there is call of non-noexcept function within noexcept-specified}}
{
    SomeStruct().throw_string("from template"); // expected-note {{non-noexcept function call is here}}
}

void call_with_assertion() noexcept
{
    assert(true);
    assert(false);
}

void call_new() noexcept // expected-warning {{there is call of non-noexcept function within noexcept-specified}}
{
    const char * str = new char[4]{"lol"}; // expected-note {{non-noexcept function call is here}}
}

void call_new_with_catch_three_dots() noexcept
{
    try {
        const char * str = new char [5]{"lmao"};
    }
    catch (...) {
        // IGNORE
    }
}

void call_new_with_catch_exception() noexcept
{
    try {
        const auto * magic_number_ptr = new int(42);
    }
    catch (std::exception const& e) {
        // IGNORE
    }
}

void call_memcpy() noexcept
{
    const char src[] = "lol";
    uint32_t dst;
    memcpy(&dst, src, sizeof(dst));
}

template <class T>
struct StructTemplate
{
    T m_val;
    StructTemplate() = default;

    StructTemplate(const StructTemplate &) = default;
};

void call_default_noexcept_ctor() noexcept
{
    SomeStruct ss;
    StructTemplate<SomeStruct> st1;
    StructTemplate<SomeStruct> st2(st1);
}

void lambda_with_new() noexcept
{
    const auto x = [=](){return new int(42);};
}

void call_defaulted_assign(const SomeStruct) noexcept
{
    SomeStruct ss, ss1;
    ss1 = ss;
}

template <class T>
void conditional_noexcept_template(T && t) noexcept(noexcept(t.method()))
{
    t.method();
}

void call_conditional_noexcept_template() noexcept
{
    conditional_noexcept_template(SomeStruct());
}

struct ThrowingMethodStruct
{
    void method();
};

void call_conditional_noexcept_false_template()
{
    conditional_noexcept_template(ThrowingMethodStruct());
}

using key = int;
using value = int;
using pair = std::pair<key, value>;

pair call_pair_ctor()
{
    pair p;
    pair p1(std::exchange(p, pair{}));
    return p1;
}

uint64_t get_time() { return 0; }

void construct_duration()
{
    const auto duration = std::chrono::microseconds(20ull - get_time());
}

// multiple decls tests
int foo() noexcept; // ok

int foo() noexcept; // ok

int bar() noexcept; // ok

int foo() noexcept // expected-warning {{there is call of non-noexcept function within noexcept-specified}}
{
    int * a = new int(42); // expected-note {{non-noexcept function call is here}}
    return *a;
}