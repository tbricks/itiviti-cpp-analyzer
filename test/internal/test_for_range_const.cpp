#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

void empty_traverse()
{
    std::vector<std::pair<std::pair<int, int>, int>> v;
    for (auto & el : v) {} // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}

    for(auto [f, s] : v) {} // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}

    std::vector<std::vector<int>> v_lines;
    for (auto & line : v_lines) {} // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}
}

void modify_in_loop()
{
    std::vector<int> vi;
    for (auto & el : vi) {
        el += 1;
    }

    std::vector<int> other;
    for (auto el : vi) { // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}
        other.push_back(el + 3);
    }
}

void const_manipulations()
{
    std::vector<int> vi;
    std::vector<int> other;

    for (auto & el : vi) { // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}
        int a = el;
        other[el] = 0;
        other[el + -a] = 3;
    }

    std::vector<std::vector<int>> vvi;
    int sum = 0;

    for (auto & line : vvi) { // TODOexpected-warning {{'const' should be specified explicitly for variable in for-range loop}}
        for (auto & el : line) { // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}
            sum += el;
        }
    }

    for (auto & line : vvi) { // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}
        for (const auto & el : line) {
            sum += el;
        }
    }

    for (auto & line : vvi) { // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}
        auto size = line.size();
        int fst = line[0];
    }

    std::vector<decltype(vi)::iterator> vits;
    for (auto & it : vits) { // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}
        int a = *it;
    }
}

void non_const_manipulations()
{
    std::vector<int> vi;
    std::vector<decltype(vi)::iterator> vits;

    for (auto & it : vits) {
        vi[0] = *(it++);
    }

    for (auto & it : vits) {
        *it = 4;
    }

    std::vector<decltype(vits)> vvits;
    for (auto & it_line : vvits) {
        for (auto & it : it_line) {
            it++;
        }
    }

    std::vector<std::vector<int>> vvi;
    for (auto & line : vvi) {
        bool cond;
        line.pop_back();
    }
}

// test const-params

void oll_korrect(const int & i)
{}

void even_unused(int & i) // expected-warning {{'i' can have 'const' qualifier}}
{}

void unused_rvalue(int && i) // expected-warning {{'i' is got as rvalue-reference, but never modified}}
{}

template <class Key, class Value>
struct SmallMap
{
    void emplace(Key && k, Value && v)
    {
        stg.reset(new value_type(std::forward<Key>(k), std::forward<Value>(v)));
    }

    template <class... Args>
    void try_emplace(int hint, Key && k, Args && ... args)
    {
        stg.reset(new value_type(std::piecewise_construct,
                                 std::forward_as_tuple(std::forward<Key>(k)),
                                 std::forward_as_tuple(std::forward<Args>(args)...)));
    }

    using value_type = std::pair<Key, Value>;
    std::unique_ptr<value_type> stg;
};

void instantiate()
{
    SmallMap<int, int> map;
    map.emplace(1, 2);
    map.try_emplace(0, 1, 2);
}

struct Movable
{
    Movable (Movable & other) // expected-warning {{'other' can have 'const' qualifier}}
        : m_data(other.m_data)
    {}
    Movable (const Movable & other)
        : m_data(other.m_data)
    {}
    Movable (Movable && other)
        : m_data(std::move(other.m_data))
    {}
    Movable & operator = (Movable && rhs) // expected-warning {{'rhs' is got as rvalue-reference, but never modified}}
    {
        m_data = rhs.m_data;
        return *this;
    }

    std::string m_data;
};


struct Abstract
{
    virtual void foo(int & a) = 0;
};

struct Derived : Abstract
{
    void foo(int & a) override {}
};

int * return_ptr(std::vector<int> & vec) {
    for (int & el : vec) {
        if (el > 4)
            return &el;
    }
    return nullptr;
}

const int * return_const_ptr(std::vector<int> & vec) {
    for (int & el : vec) // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}
        if (el > 4)
            return &el;
    return nullptr;
}

int & return_ref(std::vector<int> & vec) {
    for (int & el : vec) {
        if (el > 4)
            return el;
    }
    return vec.back();
}

const int & return_const_ref(std::vector<int> & vec) {
    for (int & el : vec) { // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}
        if (el > 4)
            return el;
    }
    return vec.back();
}

const int & return_ptr_from_lambda_and_const_ref_from_func(std::vector<int> & vec) {
    auto lambda = [](auto & vec) -> typename std::decay_t<decltype(vec)>::value_type * {
        for (auto & el : vec)
            if (el > 42)
                return &el;
        return nullptr;
    };
    lambda(vec);

    return vec.front();
}

const int & return_const_ptr_from_lambda_and_from_func(std::vector<int> & vec) {
    auto lambda = [](auto & vec) -> const int * {
        for (auto & el : vec) { // expected-warning {{'const' should be specified explicitly for variable in for-range loop}}
            if (el > 42)
                return &el;
        }
        return nullptr;
    };
    lambda(vec);

    return vec.front();
}
