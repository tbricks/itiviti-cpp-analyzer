#include <map>
#include <string>
#include <unordered_map>
#include <vector>

void test1()
{
    std::map<int, std::string> mis;
    mis.emplace(1, std::string()); // expected-warning {{use of 'emplace' with default value makes extra default constructor call}}
    mis.emplace(1, std::string{}); // expected-warning {{use of 'emplace' with default value makes extra default constructor call}}
    mis.emplace(1, std::string("word"));
    mis.emplace(1, std::string{'w'});

    std::string s;
    mis.emplace(2, s);
    mis.emplace(2, std::move(s));
}

void test2()
{
    std::unordered_map<int, std::map<int, std::pair<int, int>>> hmim;
    using Value = decltype(hmim)::mapped_type;

    hmim.try_emplace(1, std::map<int, std::pair<int, int>>()); // expected-warning {{use of 'try_emplace' with default value makes extra default constructor call}}
    hmim.try_emplace(1);

    hmim.emplace(1, Value{  }); // expected-warning {{use of 'emplace' with default value makes extra default constructor call}}
    auto [iter, new_key] = hmim.emplace(1, std::map<int, std::pair<int, int>>{{1,{2,3}}, {4,{5,6}}});

    hmim.find(1)->second.emplace(2, std::pair<int, int>( )); // expected-warning {{use of 'emplace' with default value makes extra default constructor call}}
    hmim.find(1)->second.emplace(2, std::pair<int, int>{   }); // expected-warning {{use of 'emplace' with default value makes extra default constructor call}}

    std::map<int, std::pair<int,int>> mii{{7, {8, 9}}};

    hmim.emplace(2, mii);
    hmim.emplace_hint(iter, 111, Value()); // expected-warning {{use of 'emplace_hint' with default value makes extra default constructor call}}
}

void test3()
{
    std::vector<std::pair<int, int>> vii;
    vii.emplace(vii.begin(), std::pair<int, int>());
    vii.emplace(vii.begin(), 1, 2);
}

void test4()
{
    std::map<int, int> mii;
    mii.emplace(std::make_pair(1, 2));
    mii.emplace(std::pair<int, int>(3, 4));
}

struct S {
    std::map<std::string, std::string> mis;

    void test5() {
        mis.emplace("empty", std::string()); // expected-warning {{use of 'emplace' with default value makes extra default constructor call}}
        mis.emplace("3", "three");
    }
};