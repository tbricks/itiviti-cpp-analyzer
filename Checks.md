# ICA checks

<table>
    <tr>
        <th>Check name</th>
        <th>Description</th>
        <th>Example</th>
    </tr>
    <tr>
        <th><h4>bad-rand</h4></th>
        <td>
            Detects:
            <li>usage of <code>std::rand</code> (which is not thread-safe)</li>
            <li>usage of <code>std::random_shuffle</code> (deprecated in C++14, removed in C++17)
            <li>using constants as seed</li>
            <li>generation of multiple numbers by <code>std::random_device</code> (which may be a long operation as it's execution time isn't specified)</li>
            <li>lack of <code>std::*_distribution</code> usage</li>
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
        <th><h4>char-in-ctype-pred</h4></th>
        <td>
            Detects when <code>char</code> or <code>signed char</code> is passed to the predicate functions from <i>&lt;cctype&gt;</i> (or <i>&lt;ctype.h&gt;</i>), which may be undefined befavior. From <a href="https://en.cppreference.com/w/cpp/string/byte/isalpha">cppreference</a><br><br>"Like all other functions from <i>&lt;cctype&gt;</i>, the behavior of <code>std::isalpha</code> is undefined if the argument's value is neither representable as <code>unsigned char</code> nor equal to EOF. To use these functions safely with plain <code>chars</code> (or <code>signed chars</code>), the argument should first be converted to <code>unsigned char</code>"
        </td>
        <td>
            <pre lang="cpp">
char c = getchar();
bool b1 = std::isalpha(c); // warning
bool b2 = std::isalpha(static_cast&lt;unsigned char&gt;(c)); // OK
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>const-cast-member</h4></th>
        <td>
            Detects if <code>const_cast</code> is applied to a non-static field. Usage of <code>mutable</code> is preferred
        </td>
        <td>
            <pre lang="cpp">
struct Struct
{
    int get_num() const
    {
        std::uniform_int_distribution<int> dist(0, 10);
        return dist(const_cast&lt;std::mt19937 &&gt;(m_rand_engine)); // warning
    }
private:
    std::mt19937 m_rand_engine; // assume it's properly initialized
};
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>const-param</h4></th>
        <td>
            Detects non-modified non-<code>const</code> function parameters and suggests to insert <code>const</code>.
        </td>
        <td>
            <pre lang="cpp">
void ok(const int & i) {} // OK
void param_could_be_const(int & i) {} // warning
void rvalue_is_never_moved(int && i) {} // warning
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
std::unordered_map&lt;int, std::string&gt; mis;
mis.emplace(0, std::string{}); // warning
mis.try_emplace(0); // OK
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
std::map&lt;int, bool&gt; m;
</br>auto it = m.begin(), end = m.end();
for (; it != end; ++it) {
    if (it-&gt;first == 123) {
        m.erase(it); // warning
    }
}
</br>auto it = m.begin(), end = m.end();
for (; it != end; ++it) {
    if (it-&gt;first == 123) {
        m.erase(it); // OK
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
   map.insert({key, value});
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
        <th><h4>for-range-const</h4></th>
        <td>
            Detects if `const` qualifier can be added to initialization stmt in for-range loop. Doesn't warn if the variable is used for template deduction
        </td>
        <td>
            <pre lang="cpp">
std::vector&lt;int&gt; from;
std::vector&lt;int&gt; to;
</br>for (auto & el : from) { // warning
    to.push_back(el); // causes warning
}
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>improper-move</h4></th>
        <td>
            This check extends clang-tidy: [performance-move-const-arg](https://clang.llvm.org/extra/clang-tidy/checks/performance-move-const-arg.html).
            </br>Detects:
            <li>move from non-<code>const</code> lvalue (may invalidate provided data)</li>
            <li>if <code>std::move</code> argument is already rvalue</li>
            <li>move to <code>const</code> lvalue reference</li>
            <li><code>const</code> rvalue parameters</li>
        </td>
        <td>
            <pre lang="cpp">
template &lt;class T&gt;
auto get_rvalue(T & t) { return std::move(t); } // 'move' invalidates 't'
</br>void foo(const std::string & s)
{
    auto substr = std::move(s.substr(1)); // warning: move of rvalue
    foo(std::move(substr)); // warning: move to const lvalue reference
}
</br>void boo(const std::string && s) {} // warning: const rvalue parameter
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>init-members</h4></th>
        <td>
            Detects if member can be initialized in initialization list in constructor instead of assignment within body.
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
    std::vector&lt;int&gt; m_data;
    std::size_t m_size;
};
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>inline-methods-in-class</h4></th>
        <td>
            Class method is <code>inline</code> by default, so an explicit keyword isn't needed.
            </br>Detects, when an <code>inline</code> keyword is used for a method within class body.
        </td>
        <td>
            <pre lang="cpp">
class Foo {
public:
    inline void method();  // warning
    void another_method(); // OK
};
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>move-string-stream</h4></th>
        <td>
            In C++20 standard <code>str</code> on <code>std::stringstream &&</code> got it's own overload, which should be cheaper then just copy if internal buffer.
            </br>Detects calls to <code>std::stringstream::str</code> where <code>std::move</code> could be used and suggests it.
        </td>
        <td>
            <pre lang="cpp">
void foo(std::string);
...
{
    std::stringstream ss;
    ss &lt;&lt; "Hello, world!\n";
    foo(ss.str()); // warning
}
{
    std::stringstream ss;
    ss &lt;&lt; "Goodbye, world!\n";
    foo(std::move(ss).str()); // OK
}
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>redundant-noexcept</h4></th>
        <td>
            Checks <code>noexcept</code> specified functions for calls of potentially throwing functions which bodies aren't seen in current translation unit, as it causes insertion of <code>std::terminate</code>
        </td>
        <td>
            <pre lang="cpp">
void call_new() noexcept // warning
{
    const char * str = new char[4]{"lol"}; // warning: 'new' is potentially throwing
}
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>release-lock</h4></th>
        <td>
            Detects <code>(unique|shared)_lock::release</code> without <code>unlock</code> call on returned mutex.
        </td>
        <td>
            <pre lang="cpp">
void bad_func()
{
    std::mutex m;
    std::unique_lock l{m};
</br>l.release(); // warning
}
</br>void good_func()
{
    std::mutex m;
    std::unique_lock l{m};
</br>   auto m_ptr = l.release(); // OK (unlocked afterwards)
    m_ptr->unlock();
}
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>remove-c_str</h4></th>
        <td>
            Detects if a class method that accepts <code>const char *</code> is called with calling <code>std::string::c_str</code> and there is an overload that accepts <code>std::string_view</code>. Suggests removing <code>c_str</code> call;
        </td>
        <td>
            <pre lang="cpp">
class Foo {
void func(const char *);
void func(std::string_view);
};
</br>Foo f;
</br>std::string key = "key";
f.func(key.c_str()); // warning
</br>f.func(key); // ok
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>return-value-type</h4></th>
        <td>
            Detects:
            <li>functions that return <code>const T</code>, suggests returning either <code>const T &</code> or <code>T</code> instead</li>
            <li>functions that do <code>return *this;</code> but their return type is <code>T</code>, suggests using <code>const T &</code> return type.</li>
        </td>
        <td>
            <pre lang="cpp">
const int foo() { return 42; } // warning
</br>struct Iterator
{
    Iterator operator ++ () // warning
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
static int c = 42; // OK, but will probably be judged
</br>static void local_func() {} // warning
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>temporary-in-ctor</h4></th>
        <td>
            Detects if a temporary variable is used instead of direct writing into non-static field.
        </td>
        <td>
            <pre lang="cpp">
struct Struct
{
    Struct(const int * arr, std::size_t size)
    {
        std::vector&lt;int&gt; to_fill; // warning
        if (arr) {
            for (std::size_t i = 0; i < size; ++i) {
                to_fill.push_back(arr[i]);
            }
            data = std::move(to_fill);
        }
    }
private:
    std::vector&lt;int&gt; data;
};
            </pre>
        </td>
    </tr>
    <tr>
        <th><h4>try_emplace-instead-emplace</h4></th>
        <td>
            In some standard library implementations <code>emplace</code> call always constructs a map node, which is inefficient.
            </br>Detects calls of map <code>emplace</code> method and suggests using <code>try_emplace</code> instead. NB! sometimes it implies construction of key temporary object, which may be more undesirable.
        </td>
        <td>
            <pre lang="cpp">
std::map&lt;int, std::string&gt; map;

map.emplace(1, "one"); // warning: will create a node first and find a place for it
map.try_emplace(1, "one"); // OK: will only construct a node if finds proper place
            </pre>
        </td>
    </tr>
</table>
