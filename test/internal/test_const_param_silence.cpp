#include <algorithm>
#include <iostream>
#include <optional>
#include <vector>

void sort_vec(std::vector<int> & v) // no warning
{
    auto beg = v.begin();
    auto end = v.end();
    std::sort(beg, end);
}

void print_vec(std::vector<int> & v) // alas! no warning
{
    std::for_each(v.begin(), v.end(), [](int a) { std::cout << a << '\n'; });
}

void modify_by_iterator(std::vector<int> & v) // no warning
{ *(v.begin() + 13) = 37; }

void resize(std::vector<int> & v) // no warning
{ v.resize(42); }

void modify_through_iterator_from_ptr_from_reference_to_reference_lol(std::vector<int> & v) // no warning
{
    auto & v1 = v;
    {
        auto * v_ptr = &v1;
        *((v_ptr + 1)->begin() + 2) = 0;
    }
}

void modify_by_index(std::vector<int> & v)
{ v[13] = 37; }

void modify_by_optional_iter(std::vector<int> & v) // expected-warning {{'v' can have 'const' qualifier}}
{
    std::optional mb_iter{v.begin()};
    *(mb_iter.value() + 13) = 37;
}

const int * add_const(int & i) // expected-warning {{'i' can have 'const' qualifier}}
{ return &i; }

int get_first(std::vector<int> & v) // expected-warning {{'v' can have 'const' qualifier}}
{ return v[0]; }
