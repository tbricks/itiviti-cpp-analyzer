struct Test
{
    inline int foo(); // expected-warning {{inline is used by default for method declarations/definitions in class body}}
    inline unsigned long long bar()  // expected-warning {{inline is used by default for method declarations/definitions in class body}}
    {
        return 0xFull;
    }
    int inline bar2() // expected-warning {{inline is used by default for method declarations/definitions in class body}}
    {
        return 0xABACABA;
    }
    int baz()
    {
        return 42;
    }
    int inline bar3(); // expected-warning {{inline is used by default for method declarations/definitions in class body}}

    template <typename X>
    inline int templated(X a) // expected-warning {{inline is used by default for method declarations/definitions in class body}}
    {
        return 42;
    }

    template <> inline int templated(bool a) // expected-warning {{inline is used by default for method declarations/definitions in class body}}
    {
        return 43;
    }
};

inline int Test::foo()
{
    return 42;
}

int Test::bar3() // expected-note {{Definition should be marked as inline instead of declaration}}
{
    return 0xAB0BA;
}

template <typename T>
struct Templated
{
    inline T foo(); // expected-warning {{inline is used by default for method declarations/definitions in class body}}
    inline T bar() // expected-warning {{inline is used by default for method declarations/definitions in class body}}
    {
        return T();
    }
    T baz();
};

template <typename T>
inline T Templated<T>::foo() {
    return T();
}

template <typename T>
inline T Templated<T>::baz() {
    return T();
}

int main() {
    Test test;
    test.templated(42);
}