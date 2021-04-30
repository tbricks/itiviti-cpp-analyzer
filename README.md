# itiviti-cpp-analyzer



# ICA checks

<table>
    <tr>
        <th>Check name</th>
        <th>Description</th>
        <th>Example</th>
    </tr>
    <tr>
        <th><h4>char-in-ctype-pred</h4></th>
        <td>
            Detects when <code>char</code> or <code>signed char</code> is passed to the predicate functions from <i>&lt;cctype&gt;</i> (or <i>&lt;ctype.h&gt;</i>), which may be undefined befavior. From <a href="https://en.cppreference.com/w/cpp/string/byte/isalpha">cppreference</a><br><br>"Like all other functions from <i>&lt;cctype&gt;</i>, the behavior of <code>std::isalpha</code> is undefined if the argument's value is neither representable as <code>unsigned char</code> nor equal to EOF. To use these functions safely with plain <code>chars</code> (or <code>signed chars</code>), the argument should first be converted to <code>unsigned char</code>"
        </td>
        <td>
            <pre lang="cpp">
char c = getchar();
bool b = std::isalpha(c); // warning
</br>int count_alphas(const std::string& s)
{
    return std::count_if(s.begin(), s.end(),
                    // static_cast<int(*)(int)>(std::isalpha)         // wrong
                    // [](int c){ return std::isalpha(c); }           // wrong
                    // [](char c){ return std::isalpha(c); }          // wrong
                         [](unsigned char c){ return std::isalpha(c); } // correct
                        );
}
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>emplace-default-value</h4></th>
        <td>
            Detects, when default-constructed value is passed to map `emplace` method and suggests to use `try_emplace` method instead
        </td>
        <td>
            <pre lang="cpp">
std::unordered_map<int, std::string> mis;
mis.emplace(0, std::string{}); // warning
mis.try_emplace(0) // OK
mis.emplace(1, std::string{"one"}); // OK
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>erase-in-loop</h4></th>
        <td>
            Detects when `erase` is called on iterator which is incremented in a for loop. Majority of containers invalidates an iterator which is erased. Does not produce a warning if `return` or `break` statement is present at the same level as `erase`.
        </td>
        <td>
            <pre lang="cpp">
std::map<int, bool> m;
</br>auto it = m.begin(), end = m.end();
for (; it != end; ++it) {
    if (it->first == 123) {
        m.erase(it); // warning
    }
}
</br>auto it = m.begin(), end = m.end();
for (; it != end; ++it) {
    if (it->first == 123) {
        m.erase(it); // no warning
        break;
    }
}
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>find-emplace</h4></th>
        <td>
            Detects `find` + `insert` pattern for STL-like containers. This is inefficient because `find` tries to find an element, traversing the container and then `insert` traverses the container one more time. In most cases this could be replaced with one `insert` or `emplace`
        </td>
        <td>
            <pre lang="cpp">
</br>// Inefficient:
auto it = map.find(key);
if (it == map.end()) {
   map.insert(key, value);
} else {
   it->second = value;
}
 </br>// Efficient:
auto [it, inserted] = map.emplace(key, value);
if (!inserted) {
   it->second = value;
}
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>redundant-noexcept</h4></th>
        <td>
            Checks `noexcept` specified functions for calls of potentially throwing functions which bodies aren't seen in current translation unit, as it causes insertion of `std::terminate`
        </td>
        <td>
            <pre lang="cpp">
void call_new() noexcept // warning
{
    const char * str = new char[4]{"lol"}; // 'new' caused warning
}
int get_min(int l, int r) noexcept
{
    return std::min(l, r); // 'std::min' is not noexcept-specified
}
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>remove-c_str</h4></th>
        <td>
            Suggests removing unnecessary `c_str()` calls if overload with `std::string_view` exists.
        </td>
        <td>
            <pre lang="cpp">
void func(const char *);
void func(std::string_view);
</br>std::string key = "key";
func(key.c_str()); // warning
</br>func(key) // ok
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>bad-rand</h4></th>
        <td>
            Detects:
            <li>usage of `std::rand` (which is not thread-safe)</li>
            <li>usage of `std::random_shuffle` (deprecated in C++14, removed in C++17)
            <li>using constants as seed</li>
            <li>generation of multiple numbers by `std::random_device` (which may be a long operation as it's execution time isn't specified)</li>
            <li>lack of `std::*_distribution` usage</li>
            <li>one-shot usage of random engine in function</li>
        </td>
        <td>
            <pre lang="cpp">
// good example
struct random_struct
{
    random_struct()
        : m_rand_engine(std::random_device{}()) // construct random device and call to get seed for random engine construction
    {}
</br>    int get_rand(int from, int to)
    {
        std::uniform_int_distribution<int> distribution(from, to); // distribution construction is quite cheap
        return distribution(m_rand_engine);
    }
private:
    mutable std::mt19937 m_rand_engine;
};
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>for-range-const</h4></th>
        <td>
            Detects if `const` qualifier can be added to initialization stmt in for-range loop. Doesn't warn if the variable is used for template deduction
        </td>
        <td>
            <pre lang="cpp">
std::vector<int> from;
std::vector<int> to;
</br>for (auto & el : from) { // warning
    to.push_back(el); // causes warning
}
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>const-param</h4></th>
        <td>
            Detects:
            <li>unnecessary absence of `const` qualifier of function parameter</li>
            <li>non-modified parameters which were got by rvalue-reference</li>
        </td>
        <td>
            <pre lang="cpp">
void ok(const int & i) {}
void param_could_be_const(int & i) {} // warning
void rvalue_is_never_moved(int && i) {} // warning
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>improper-move</h4></th>
        <td>
            Detects:
            <li>move from non-`const` lvalue (may invalidate provided data)</li>
            <li>if `std::move` argument is already rvalue</li>
            <li>move to `const` lvalue reference</li>
            <li>move of trivial types (it's identical to copy)</li>
            <li>`const` rvalue parameters</li>
        </td>
        <td>
            <pre lang="cpp">
template <class T>
auto get_rvalue(T & t) { return std::move(t); } // 'move' invalidates 't'
</br>void foo(const std::string s)
{
    auto substr = s.substr(1); // move of rvalue
    foo(std::move(substr)); // move to const lvalue
    int a = 3, b;
    b = std::move(a) // move of trivial type
}
</br>void boo(const std::string && s) {} // const rvalue parameter
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>init-members</h4></th>
        <td>
            Detects if member can be initialized in initialization list in constructor instead of assignment within body
        </td>
        <td>
            <pre lang="cpp">
struct Struct
{
    Struct (const int * arr, std::size_t size)
        : m_data(arr, arr + size) // good
    {
        m_size = size; // bad, warning
    }
private:
    std::vector<int> m_data;
    std::size_t m_size;
};
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>correct-iterator</h4></th>
        <td>
            Detects if an iterator type doesn't declare all needed members
        </td>
        <td>
            <pre lang="cpp">
// good iterator
struct iterator
{
</br>    using value_type = long;
    using reference = value_type &;
    using pointer = value_type *;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;
</br>    iterator(const iterator &) = default;
    iterator & operator = (const iterator &) = default;
</br>    reference operator * ();
    pointer operator -> ();
</br>    iterator & operator ++ ();
    iterator operator ++ (int);
</br>    iterator & operator -- ();
    iterator operator -- (int);
</br>    bool operator == (const iterator &) const;
    bool operator != (const iterator &) const;
};
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>const-cast-member</h4></th>
        <td>
            Detects if `const_cast` is applied to member. Usage of `mutable` is preferred
        </td>
        <td>
            <pre lang="cpp">
struct Struct
{
    int get_num() const
    {
        std::uniform_int_distribution<int> dist(0, 10);
        return dist(const_cast<std::mt19937>(m_rand_engine)); // warning
    }
private:
    std::mt19937 m_rand_engine; // assume it's properly initialized
};
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>temporary-in-ctor</h4></th>
        <td>
            Detects if temporary used instead of direct writing into member
        </td>
        <td>
            <pre lang="cpp">
struct Struct
{
    Struct(const int * arr, std::size_t size)
    {
        std::vector<int> to_fill; // warning
        if (arr) {
            for (std::size_t i = 0; i < size; ++i) {
                to_fill.push_back(arr[i]);
            }
            data = std::move(to_fill);
        }
    }
private:
    std::vector<int> data;
};
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>static-keyword</h4></th>
        <td>
            Detects:
            <li>global variables in headers (may cause problems with initialization ordering)</li>
            <li>declarations of functions, local for translation unit</li>
        </td>
        <td>
            <pre lang="cpp">
// file.h
#pragma once
</br>inline static int a; // warning
</br>namespace { int b; } // warning
</br>// file.cpp
static int c = 42; // will probably be judged, but ok
</br>static void local_func() {} // warning
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>return-value-type</h4></th>
        <td>
            Detects:
            <li>if function returns `const T`, suggests returning either `const T &` or `T` instead</li>
            <li>if member function return type is `T`, but it only returns `*this`, suggests using `const T &`</li>
        </td>
        <td>
            <pre lang="cpp">
const int foo() { return 42; } // warning
</br>struct Iterator
{
    Iterator operator ++ ()
    {
        inc();
        return *this;
    }
private:
    void inc();
};
            </pre>
        </td>
    </tr>
</table>
