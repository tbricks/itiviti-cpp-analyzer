#include <map>

using Map = std::map<int, bool>;

Map & get_map()
{
    static Map _map;
    return _map;
}

void f1()
{
    const int id = 1;
    Map mmm;
    const auto it = mmm.find(id); // expected-note {{'find' called here}}
    if (it == mmm.end()) {
        mmm[id] = true; // expected-warning {{'operator[]' called after find(). Consider using 'const auto [it, inserted] = mmm.try_emplace(id, ...)'}}
    }
}

void f2()
{
    const int id = 1;
    const auto itabc = get_map().find(id); // expected-note {{'find' called here}}
    if (get_map().end() == itabc) {
        get_map()[id] = true; // expected-warning {{'operator[]' called after find(). Consider using 'const auto [itabc, inserted] = get_map().try_emplace(id, ...)'}}
    }
}

int get_id()
{
    return 123;
}

void f3()
{
    Map m;
    if (const auto it = m.find(get_id()); it != m.end()) { // expected-note {{'find' called here}}
    } else {
        m[get_id()] = true; // expected-warning {{'operator[]' called after find(). Consider using 'const auto [it, inserted] = m.try_emplace(get_id(), ...)'}}
    }
}

void f4()
{
    Map m;
    if (const auto it = m.find(get_id()); it != m.end()) { // expected-note {{'find' called here}}
    } else {
        m.emplace(get_id(), true); // expected-warning {{'emplace' called after find(). Consider using 'const auto [it, inserted] = m.try_emplace(get_id(), ...)'}}
    }
}

void f5(const int & id)
{
    const auto it = get_map().find(id); // expected-note {{'find' called here}}
    if (get_map().end() == it) {
        get_map()[id] = true; // expected-warning {{'operator[]' called after find(). Consider using 'const auto [it, inserted] = get_map().try_emplace(id, ...)'}}
    }
}

void f6()
{
    const int id = 1;
    Map mmm;
    const auto it = mmm.find(id); // expected-note {{'find' called here}}
    if (it == mmm.end()) {
        mmm.emplace(id, true); // expected-warning {{'emplace' called after find(). Consider using 'const auto [it, inserted] = mmm.try_emplace(id, ...)'}}
    }
}

int main()
{
    return 0;
}
