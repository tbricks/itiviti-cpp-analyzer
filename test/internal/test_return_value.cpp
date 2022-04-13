struct TestStruct
{
    const int method(); // expected-warning {{'method' returns const value. Consider using 'const int &' or 'int' instead}}

    TestStruct get_val() // expected-warning {{'get_val' always returns copy of '*this', consider returning 'const TestStruct &'}}
    {
        return *this;
    }

    const TestStruct get_const_val() // expected-warning {{'get_const_val' returns const value. Consider using 'const TestStruct &' instead}}
                                     // expected-warning@-1 {{'get_const_val' always returns copy of '*this', consider returning 'const TestStruct &'}}
    {
        return *this;
    }

    TestStruct return_other()
    {
        return TestStruct{3};
    }

    TestStruct return_this_or_other()
    {
        if (bool a; a) {
            return *this;
        } else {
            return TestStruct{42};
        }
    }

    int get_field()
    {
        return field;
    }

    int field;
};

template <class T>
struct TemplateStruct
{
    TemplateStruct get_val() // expected-warning {{'get_val' always returns copy of '*this', consider returning 'const TemplateStruct<T> &'}}
    {
        return *this;
    }

    const TemplateStruct get_const_val() // expected-warning {{'get_const_val' returns const value. Consider using 'const TemplateStruct<T> &' instead}}
                                         // expected-warning@-1 {{'get_const_val' always returns copy of '*this', consider returning 'const TemplateStruct<T> &'}}
    {
        return *this;
    }

    TemplateStruct return_other()
    {
        return TemplateStruct{};
    }

    TemplateStruct return_this_or_other()
    {
        if (bool a; a) {
            return *this;
        } else {
            return TemplateStruct{};
        }
    }
};

template struct TemplateStruct<int>;

void void_func()
{
    return;
}

void no_return() {}

int folded_func()
{
    int a = 3;
    auto folded = [&] { return a; };
    return folded();
}

int main()
{
    int a;
    a -= a;
}
