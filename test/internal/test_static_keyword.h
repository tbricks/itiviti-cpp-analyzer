#pragma once

inline int singleton()
{
    constexpr auto lambda = []{ return 3; };
    static int value = lambda();
    return value;
}

static int a; // expected-warning {{global var in header}}

namespace {
    inline int b = 3; // expected-warning {{global var in header}}
}

static constexpr int c = 42;
static const int const_val = 11;
static constexpr auto str = "str";

struct ValueHolder
{
    static constexpr int value = 0;
};

inline int foo()
{
    int c, d;
    return c + d;
}

template <class T>
struct TemplatedStruct
{
    static inline int value = 42;
};
template struct TemplatedStruct<int>;
