#include <type_traits>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <string>
#include <utility>

struct NonMovable
{
    std::string a = "42";
    NonMovable(NonMovable &&) = delete;
    NonMovable(const NonMovable &) = default;
    NonMovable(NonMovable &) = default;
    NonMovable() = default;
    NonMovable(const std::string & b) : a(b) {}
};

struct NonMovableAndCopiable
{
    std::string a = "0";
    NonMovableAndCopiable(NonMovableAndCopiable &&) = delete;
    NonMovableAndCopiable(const NonMovableAndCopiable &) = delete;
    NonMovableAndCopiable(NonMovableAndCopiable &) = delete;
    NonMovableAndCopiable() = default;
    NonMovableAndCopiable(std::string_view sv) : a(sv) {}
};

bool operator<(const NonMovable & a, const NonMovable & b)
{
    return a.a < b.a;
}

bool operator<(const NonMovableAndCopiable & a, const NonMovableAndCopiable & b)
{
    return a.a < b.a;
}

int main()
{
    std::unordered_map <int, int> m;
    std::unordered_set <int> s;
    std::unordered_map <std::string, int> t;
    std::map <NonMovable, int> r;
    std::map <NonMovableAndCopiable, int> q;
    m.emplace(0, 0); // expected-warning {{'try_emplace' could be used instead of 'emplace'.}}
    s.emplace(0); // ok
    t.emplace("abc", 123); // expected-warning {{'try_emplace' could be used instead of 'emplace'.}}
    t.emplace(std::piecewise_construct, std::forward_as_tuple(10, 'c'), std::forward_as_tuple(10)); // ok
    t.emplace(std::piecewise_construct_t{}, std::forward_as_tuple(10, 'c'), std::forward_as_tuple(10)); // ok
    t.try_emplace("123", 42); // ok
    t.emplace_hint(t.begin(), "123", 42); // expected-warning {{'try_emplace' could be used instead of 'emplace_hint'.}}
    t.emplace_hint(t.begin(), std::piecewise_construct, std::forward_as_tuple(10, 'c'), std::forward_as_tuple(10)); // ok
    NonMovable a;
    static_assert(std::is_copy_constructible<decltype(a)>() &&
                  !std::is_move_constructible<decltype(a)>());
    r.emplace(a, 42); // expected-warning {{'try_emplace' could be used instead of 'emplace'.}}
    static_assert(!std::is_copy_constructible<NonMovableAndCopiable>() &&
                  !std::is_move_constructible<NonMovableAndCopiable>());
     q.emplace("42", 42); // literally ok
}