#include <map>
#include <string>

struct Test
{
    std::map<int, int> m_map;
};

struct Test2
{
    std::map<int, int> m_map;
};

struct Key
{
    int id() const { return 1; }

    void test1()
    {
        std::string s_one = "one";
        std::string s_two = "two";

        auto it_one = msi.find(s_one); // expected-note 5 {{'find' called here}}

        if (it_one == msi.end()) {
            msi.emplace(s_one, 1); // expected-warning {{'emplace' called after find().}}
            msi.insert(std::make_pair(s_one, 1)); // expected-warning {{'insert' called after find()}}
            msi.insert({s_one, 1}); // expected-warning {{'insert' called after find()}}
            msi.insert(std::pair<std::string, int>(s_one, 1)); // expected-warning {{'insert' called after find()}}
            msi.emplace(s_two, 2);
            msi[s_one] = 1; // expected-warning {{'operator[]' called after find().}}
            msi[s_two] = 2;
        }
    }

    void test2()
    {
        auto it_one = msi.find("one"); // expected-note 3 {{'find' called here}}

        msi.emplace("one", 1); // expected-warning {{'emplace' called after find().}}
        msi.insert(std::make_pair("one", 1)); // expected-warning {{'insert' called after find().}}
        msi.emplace("two", 2);
        msi["one"] = 1; // expected-warning {{'operator[]' called after find().}}
    }

    void test3() {
        auto it = msi.find("one");
    }

    void test4() {
        msi.emplace("one", 1);
    }

    std::map<std::string, int> msi;
};

int main()
{
    const int abc = 10;
    bool cond;

    std::map<int, int> map;

    if (cond) {
        const auto it = map.find(abc); // expected-note 2 {{'find' called here}}
        if (it == map.end()) {
            map.emplace(abc, 100); // expected-warning {{'emplace' called after find().}}
            map[abc] = 100; // expected-warning {{'operator[]' called after find().}}
        }
    } else {
        map.emplace(abc, 111);
        map[abc] = 111;
    }

    Key k;
    {
        const auto k_it = map.find(k.id()); // expected-note {{'find' called here}}
        if (k_it == map.end()) {
            map[k.id()] = 123; // expected-warning {{'operator[]' called after find().}}
        }
    }
    {
        map[k.id()] = 123;
    }

    Test test;
    Test2 * ptr = new Test2;

    while (cond) {
        if (const auto it = test.m_map.find(1); it == test.m_map.end()) { // expected-note 2 {{'find' called here}}
            test.m_map[1] = 2; // expected-warning {{'operator[]' called after find().}}
        }
        if (const auto it = test.m_map.find(1); it == test.m_map.end()) {
            test.m_map.emplace(1, 2); // expected-warning {{'emplace' called after find().}}
        }

        if (const auto it = ptr->m_map.find(1); it == ptr->m_map.end()) { // expected-note 2 {{'find' called here}}
            ptr->m_map[1] = 2; // expected-warning {{'operator[]' called after find()}}
        }
        if (const auto it = ptr->m_map.find(1); it == ptr->m_map.end()) {
            ptr->m_map.insert({1, 2}); // expected-warning {{'insert' called after find().}}
        }
    }
    test.m_map[1] = 2;
    test.m_map.emplace(1, 2);
    ptr->m_map[1] = 2;
    ptr->m_map.insert({1, 2});

    {
        auto it = map.find(1);
    }
    {
        map.emplace(1, 2);
    }

    return 0;
}
