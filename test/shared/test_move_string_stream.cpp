#include <sstream>

void foo(std::string s) {}

int main()
{
    std::stringstream ss;

    ss << "qweqwe" << 1 << 12.;

    foo(ss.str());

    ss << "qwe";

    {
        std::stringstream ss1;
        foo(ss1.str()); // expected-warning {{add std::move() to last std::stringstream usage to avoid allocation in C++20}}
    }
}

struct S
{
    void foo()
    {
        std::stringstream ss;
        ::foo(ss.str()); // expected-warning {{add std::move() to last std::stringstream usage to avoid allocation in C++20}}
    }

};
