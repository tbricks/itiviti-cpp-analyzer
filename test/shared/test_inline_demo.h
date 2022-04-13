struct Foo
{
    inline int bar(); // expected-warning {{inline is used by default for method declarations/definitions in class body}}
    int inline baz(); // expected-warning {{inline is used by default for method declarations/definitions in class body}}
    int good_method();
    int another_one_good_method();
};


inline int Foo::bar()
{
    return 42;
}

int Foo::baz() // expected-note {{Definition should be marked as inline instead of declaration}}
{
    return 42;
}

inline int Foo::good_method()
{
    return 42;
}