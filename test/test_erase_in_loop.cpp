#include <map>
#include <iostream>

struct Cont
{
    template <class T>
    void erase(const T &) { }
};

int main()
{
    Cont c;
    for (int i = 0; i != 10; ++i) {
        c.erase(i);
    }

    std::map<int, bool> m;
    {
        auto it = m.begin(), end = m.end();
        for (; it != end; ++it) { // expected-note {{increment called here}}
            if (it->first == 123) {
                m.erase(it); // expected-warning {{erase() invalidates 'it', which is incremented in a for loop}}
            }
        }
    }

    {
        auto it = m.begin(), end = m.end();
        for (; it != end; it++) {
            if (it->first == 123) {
                m.erase(it);
                break;
            }
        }
    }

    {
        auto it = m.begin(), end = m.end();
        for (; it != end; it++) {
            if (it->first == 123) {
                m.erase(it);
                std::cout << 1;
                return 123;
            }
        }
    }
}
