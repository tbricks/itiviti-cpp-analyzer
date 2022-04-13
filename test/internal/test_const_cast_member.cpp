#include <cstdint>
#include <string>

struct TestStruct
{
    unsigned cnt = 0;
    std::string str;
    char * ptr;

    void simple_cast() const
    {
        ++const_cast<unsigned &>(cnt); // expected-warning {{It is better to declare 'cnt' as 'mutable' instead of 'const_cast'}}
    }

    void call_method() const
    {
        const_cast<std::string &>(str).clear(); // expected-warning {{It is better to declare 'str' as 'mutable' instead of 'const_cast'}}
    }

    void to_const()
    {
        const_cast<const std::string &>(str)[0];
    }

    void cast_ptr() const
    {
        const_cast<char *>(ptr)[2] = 'a'; // expected-warning {{It is better to declare 'ptr' as 'mutable' instead of 'const_cast'}}
    }

};