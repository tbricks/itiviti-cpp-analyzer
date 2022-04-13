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
        mmm.insert({id, true}); // expected-warning {{'insert' called after find(). Consider using 'const auto [it, inserted] = mmm.try_emplace(id, ...)}}
    }
}

void f11()
{
    const int id = 1;
    Map mmm;
    const auto it = mmm.find(id); // expected-note {{'find' called here}}
    if (it == mmm.end()) {
        mmm.insert(std::pair{id, true}); // expected-warning {{'insert' called after find(). Consider using 'const auto [it, inserted] = mmm.try_emplace(id, ...)}}
    }
}



void f2()
{
    const int id = 1;
    const auto itabc = get_map().find(id); // expected-note {{'find' called here}}
    if (get_map().end() == itabc) {
        get_map().insert(std::make_pair(id, true)); // expected-warning {{'insert' called after find(). Consider using 'const auto [itabc, inserted] = get_map().try_emplace(id, ...)}}
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
        std::pair p = {get_id(), true};
        m.insert(p); // expected-warning {{'insert' called after find(). Consider using 'const auto [it, inserted] = m.try_emplace(get_id(), ...)}}
    }
}

void f4()
{
    Map m;
    if (const auto it = m.find(get_id()); it != m.end()) {
    } else {
        const auto p = std::make_pair(get_id(), false);
        m.insert(std::move(p));
    }
}

void f5(const int & id)
{
    const auto it = get_map().find(id); // expected-note {{'find' called here}}
    if (get_map().end() == it) {
        const auto p = std::pair{id, true};
        get_map().insert(p); // expected-warning {{'insert' called after find(). Consider using 'const auto [it, inserted] = get_map().try_emplace(id, ...)}}
    }
}

int main()
{
    return 0;
}
