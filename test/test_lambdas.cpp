#include <vector>
#include <iostream>
#include <type_traits>

template <class T>
struct NamePrinter;

void test()
{
    std::vector<int> ints;
    int i = 1;

    auto foo = [&ints, i ] () {
        //std::cout << std::is_lvalue_reference_v<decltype(ints)> << '\n';
        //NamePrinter<decltype(ints)> np;
        std::cout << ints.size() << '\n';
    };
    ints.push_back(1);
    foo();
}


int main()
{
    test();
    return 0;
}
